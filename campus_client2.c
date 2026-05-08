#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define TCP_PORT 8080
#define UDP_PORT 9090
#define BUFFER_SIZE 1024
#define HEARTBEAT_INTERVAL 10
#define MAX_MESSAGES 50

typedef struct {
    char from_campus[50];
    char from_dept[50];
    char to_campus[50];
    char to_dept[50];
    char content[500];
    time_t timestamp;
} Message;

int tcp_socket_fd, udp_socket_fd;
char campus_name[50], campus_password[50];
int running = 1;

Message inbox[MAX_MESSAGES];
int inbox_count = 0;
pthread_mutex_t inbox_mutex = PTHREAD_MUTEX_INITIALIZER;

const char* departments[] = {"Admissions", "Academics", "IT", "Sports"};
const int num_departments = 4;

void* tcp_receiver(void* arg) {
    char buffer[BUFFER_SIZE];
    while(running) {
        memset(buffer,0,BUFFER_SIZE);
        int recv_len = recv(tcp_socket_fd, buffer, BUFFER_SIZE-1,0);
        if(recv_len<=0){ if(running) printf("\n[ERROR] Lost connection to server!\n"); running=0; break; }
        buffer[recv_len]='\0';
        if(strncmp(buffer,"MSG:",4)==0){
            char* msg_copy = strdup(buffer);
            char* source_campus = strtok(buffer+4, ":");
            char* source_dept = strtok(NULL, ":");
            char* target_campus = strtok(NULL, ":");
            char* target_dept = strtok(NULL, ":");
            char* content = strtok(NULL, "");

            pthread_mutex_lock(&inbox_mutex);
            if(inbox_count<MAX_MESSAGES){
                strcpy(inbox[inbox_count].from_campus, source_campus);
                strcpy(inbox[inbox_count].from_dept, source_dept);
                strcpy(inbox[inbox_count].to_campus, target_campus);
                strcpy(inbox[inbox_count].to_dept, target_dept);
                strcpy(inbox[inbox_count].content, content);
                inbox[inbox_count].timestamp = time(NULL);
                inbox_count++;
            }
            pthread_mutex_unlock(&inbox_mutex);

            printf("\n\n═══════════════════════════\n");
            printf("   NEW MESSAGE RECEIVED \n");
            printf("═══════════════════════════\n");
            printf(" From: %s - %s\n", source_campus, source_dept);
            printf(" To: %s - %s\n", target_campus, target_dept);
            printf(" Message: %s\n", content);
            printf("═══════════════════════════\n");
            printf("%s Campus> ",campus_name);
            fflush(stdout);
            free(msg_copy);
        }
        else if(strncmp(buffer,"ERROR:",6)==0){
            printf("\n[ERROR] %s\n",buffer+6);
            printf("%s Campus> ",campus_name);
            fflush(stdout);
        }
    }
    return NULL;
}

void* udp_heartbeat_sender(void* arg) {
    struct sockaddr_in server_addr;
    char heartbeat_msg[BUFFER_SIZE];
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(UDP_PORT);
    inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr);
    sprintf(heartbeat_msg,"HEARTBEAT:%s",campus_name);
    while(running){
        sendto(udp_socket_fd,heartbeat_msg,strlen(heartbeat_msg),0,(struct sockaddr*)&server_addr,sizeof(server_addr));
        sleep(HEARTBEAT_INTERVAL);
    }
    return NULL;
}

void* udp_broadcast_receiver(void* arg){
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    while(running){
        memset(buffer,0,BUFFER_SIZE);
        int recv_len = recvfrom(udp_socket_fd, buffer, BUFFER_SIZE-1, 0, (struct sockaddr*)&sender_addr, &addr_len);
        if(recv_len>0){
            buffer[recv_len]='\0';
            if(strncmp(buffer,"BROADCAST:",10)==0){
                char* msg = buffer+10;
                printf("\n\n═══════════════════════════\n");
                printf("    SYSTEM BROADCAST    \n");
                printf("═══════════════════════════\n");
                printf(" %s\n", msg);
                printf("═══════════════════════════\n");
                printf("%s Campus> ",campus_name);
                fflush(stdout);
            }
        }
    }
    return NULL;
}

void display_menu(){
    printf("\n════════════════════════════════\n");
    printf("      %s CAMPUS - MAIN MENU     \n",campus_name);
    printf("════════════════════════════════\n");
    printf("║ 1. Send Message                ║\n");
    printf("║ 2. View Received Messages      ║\n");
    printf("║ 3. Show Menu                   ║\n");
    printf("║ 4. Exit                        ║\n");
    printf("╚════════════════════════════════╝\n");
}

void clear_input_buffer(){
    int c;
    while((c=getchar())!='\n' && c!=EOF);
}

void send_message(){
    char target_campus[50];
    char source_dept[50];
    char target_dept[50];
    char message[500];

    printf("Enter target campus: "); fgets(target_campus,sizeof(target_campus),stdin); target_campus[strcspn(target_campus,"\n")]=0;

    printf("Choose your department:\n");
    for(int i=0;i<num_departments;i++) printf("%d. %s\n",i+1,departments[i]);
    int choice; scanf("%d",&choice); clear_input_buffer();
    if(choice<1 || choice>num_departments){ printf("Invalid choice\n"); return; }
    strcpy(source_dept,departments[choice-1]);

    printf("Choose target department:\n");
    for(int i=0;i<num_departments;i++) printf("%d. %s\n",i+1,departments[i]);
    scanf("%d",&choice); clear_input_buffer();
    if(choice<1 || choice>num_departments){ printf("Invalid choice\n"); return; }
    strcpy(target_dept,departments[choice-1]);

    printf("Enter message: "); fgets(message,sizeof(message),stdin); message[strcspn(message,"\n")]=0;

    char full_msg[BUFFER_SIZE];
    sprintf(full_msg,"MSG:%s:%s:%s:%s:%s",campus_name,source_dept,target_campus,target_dept,message);
    send(tcp_socket_fd,full_msg,strlen(full_msg),0);
    printf("[SENT] Message sent to %s campus\n",target_campus);
}

void view_received_messages(){
    pthread_mutex_lock(&inbox_mutex);
    if(inbox_count==0){ printf("Inbox is empty\n"); pthread_mutex_unlock(&inbox_mutex); return; }
    for(int i=0;i<inbox_count;i++){
        char time_str[26]; struct tm* tm_info = localtime(&inbox[i].timestamp);
        strftime(time_str,26,"%Y-%m-%d %H:%M:%S",tm_info);
        printf("\nMessage #%d\nTime: %s\nFrom: %s (%s)\nTo: %s (%s)\n%s\n",
               i+1,time_str,inbox[i].from_campus,inbox[i].from_dept,inbox[i].to_campus,inbox[i].to_dept,inbox[i].content);
    }
    printf("Press Enter to continue..."); getchar();
    pthread_mutex_unlock(&inbox_mutex);
}

int main(int argc,char* argv[]){
    struct sockaddr_in server_addr, udp_addr;
    pthread_t tcp_recv_thread, udp_heartbeat_thread, udp_broadcast_thread;

    if(argc==3){ strcpy(campus_name,argv[1]); strcpy(campus_password,argv[2]); }
    else{
        printf("Available campuses: Islamabad, Lahore, Karachi, Peshawar, CFD, Multan\n");
        printf("Enter Campus Name: "); fgets(campus_name,sizeof(campus_name),stdin); campus_name[strcspn(campus_name,"\n")]=0;
        printf("Enter Campus Password: "); fgets(campus_password,sizeof(campus_password),stdin); campus_password[strcspn(campus_password,"\n")]=0;
    }

    tcp_socket_fd = socket(AF_INET,SOCK_STREAM,0); if(tcp_socket_fd<0){ perror("TCP socket failed"); exit(EXIT_FAILURE); }
    memset(&server_addr,0,sizeof(server_addr)); server_addr.sin_family=AF_INET; server_addr.sin_port=htons(TCP_PORT);
    if(inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr)<=0){ perror("Invalid server"); exit(EXIT_FAILURE); }
    if(connect(tcp_socket_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){ perror("Connect failed"); exit(EXIT_FAILURE); }

    char auth_msg[BUFFER_SIZE]; sprintf(auth_msg,"AUTH:%s:%s",campus_name,campus_password);
    send(tcp_socket_fd,auth_msg,strlen(auth_msg),0);
    char buffer[BUFFER_SIZE]; int recv_len = recv(tcp_socket_fd,buffer,BUFFER_SIZE-1,0);
    if(recv_len>0){ buffer[recv_len]='\0'; if(strcmp(buffer,"AUTH_OK")!=0){ printf("Auth failed\n"); close(tcp_socket_fd); exit(EXIT_FAILURE); } }

    udp_socket_fd = socket(AF_INET,SOCK_DGRAM,0); if(udp_socket_fd<0){ perror("UDP socket failed"); exit(EXIT_FAILURE); }
    memset(&udp_addr,0,sizeof(udp_addr)); udp_addr.sin_family=AF_INET; udp_addr.sin_addr.s_addr=INADDR_ANY; udp_addr.sin_port=0;
    if(bind(udp_socket_fd,(struct sockaddr*)&udp_addr,sizeof(udp_addr))<0){ perror("UDP bind failed"); exit(EXIT_FAILURE); }

    pthread_create(&tcp_recv_thread,NULL,tcp_receiver,NULL);
    pthread_create(&udp_heartbeat_thread,NULL,udp_heartbeat_sender,NULL);
    pthread_create(&udp_broadcast_thread,NULL,udp_broadcast_receiver,NULL);

    display_menu();
    int choice;
    while(running){
        printf("%s Campus> ",campus_name);
        if(scanf("%d",&choice)!=1){ clear_input_buffer(); continue; }
        clear_input_buffer();
        switch(choice){
            case 1: send_message(); break;
            case 2: view_received_messages(); break;
            case 3: display_menu(); break;
            case 4: running=0; break;
            default: printf("Invalid choice\n");
        }
    }

    close(tcp_socket_fd); close(udp_socket_fd); return 0;
}
