/*
 * ============================================================
 *  CAMPUS INTER-NETWORKING GUI CLIENT
 *  WhatsApp-Inspired Glassy Theme with GTK3
 * ============================================================
 *
 *  Compile:
 *    gcc campus_client_gui.c -o campus_client_gui \
 *        $(pkg-config --cflags --libs gtk+-3.0) -lpthread
 *
 *  Run:
 *    ./campus_client_gui
 * ============================================================
 */

#include <arpa/inet.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ── Network Constants ─────────────────────────────────── */
#define SERVER_IP "127.0.0.1"
#define TCP_PORT 8080
#define UDP_PORT 9090
#define BUFFER_SIZE 1024
#define HEARTBEAT_INTERVAL 10
#define MAX_MESSAGES 200

/* ── Data Structures ───────────────────────────────────── */
typedef struct {
  char from_campus[50];
  char from_dept[50];
  char to_campus[50];
  char to_dept[50];
  char content[500];
  time_t timestamp;
  int is_sent; /* 1 = we sent it, 0 = we received it */
} Message;

/* ── Global State ──────────────────────────────────────── */
static int tcp_socket_fd = -1, udp_socket_fd = -1;
static char campus_name[50] = {0};
static char campus_password[50] = {0};
static int running = 0;
static int authenticated = 0;

static Message messages[MAX_MESSAGES];
static int msg_count = 0;
/* Mutex to ensure thread-safe access to message history between network and GUI
 * threads */
static pthread_mutex_t msg_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *departments[] = {"Admissions", "Academics", "IT", "Sports"};
static const int num_departments = 4;

/* ── GTK Widgets ───────────────────────────────────────── */
static GtkWidget *main_window;
static GtkWidget *login_stack;

/* Login page */
static GtkWidget *login_page;
static GtkWidget *campus_combo;
static GtkWidget *password_entry;
static GtkWidget *login_btn;
static GtkWidget *login_status_label;
static GtkWidget *login_spinner;

/* Chat page */
static GtkWidget *chat_page;
static GtkWidget *chat_box; /* VBox inside scrolled window */
static GtkWidget *chat_scroll;
static GtkWidget *msg_entry;
static GtkWidget *send_btn;
static GtkWidget *target_campus_combo;
static GtkWidget *source_dept_combo;
static GtkWidget *target_dept_combo;
static GtkWidget *header_label;
static GtkWidget *status_label;
static GtkWidget *sidebar_online_box;
static GtkWidget *broadcast_label;

/* ───────────────────────────────────────────────────────── */
/*                   CSS – GLASSY WHATSAPP THEME            */
/* ───────────────────────────────────────────────────────── */
/* ───────────────────────────────────────────────────────── */
/*  CSS SECTION – Defines the Glassy WhatsApp-Style Theme    */
/* ───────────────────────────────────────────────────────── */
static const char *css_data =
    "/* ── Global ────────────────────────────────────────── */\n"
    "* {\n"
    "  font-family: 'Segoe UI', 'Roboto', 'Ubuntu', sans-serif;\n"
    "}\n"
    "\n"
    "window {\n"
    "  background: linear-gradient(145deg, #070d12 0%, #0b1a1f 40%, #0d2218 "
    "100%);\n"
    "}\n"
    "\n"
    "/* ── Login Page ────────────────────────────────────── */\n"
    "#login-page {\n"
    "  background: transparent;\n"
    "}\n"
    "\n"
    "#login-card {\n"
    "  background: alpha(#1a2e35, 0.65);\n"
    "  border-radius: 24px;\n"
    "  border: 1px solid alpha(#25D366, 0.15);\n"
    "  padding: 48px 42px;\n"
    "  box-shadow: 0 8px 32px alpha(black, 0.4), inset 0 1px 0 alpha(white, "
    "0.05);\n"
    "}\n"
    "\n"
    "#login-title {\n"
    "  color: #25D366;\n"
    "  font-size: 28px;\n"
    "  font-weight: 800;\n"
    "  letter-spacing: 1px;\n"
    "  margin-bottom: 6px;\n"
    "}\n"
    "\n"
    "#login-subtitle {\n"
    "  color: alpha(#8696a0, 0.8);\n"
    "  font-size: 13px;\n"
    "  margin-bottom: 24px;\n"
    "}\n"
    "\n"
    "#login-status {\n"
    "  color: #ef5350;\n"
    "  font-size: 12px;\n"
    "  margin-top: 4px;\n"
    "}\n"
    "\n"
    "/* ── Input Fields ──────────────────────────────────── */\n"
    "entry, combobox button {\n"
    "  background: alpha(#0b1a1f, 0.7);\n"
    "  border: 1px solid alpha(#25D366, 0.2);\n"
    "  border-radius: 12px;\n"
    "  color: #e9edef;\n"
    "  padding: 10px 16px;\n"
    "  font-size: 14px;\n"
    "  min-height: 20px;\n"
    "  caret-color: #25D366;\n"
    "  transition: border-color 200ms ease;\n"
    "}\n"
    "\n"
    "entry:focus, combobox button:focus {\n"
    "  border-color: #25D366;\n"
    "  box-shadow: 0 0 0 2px alpha(#25D366, 0.15);\n"
    "}\n"
    "\n"
    "entry selection {\n"
    "  background-color: alpha(#25D366, 0.3);\n"
    "  color: white;\n"
    "}\n"
    "\n"
    "/* ── Buttons ────────────────────────────────────────── */\n"
    "#login-btn {\n"
    "  background: linear-gradient(135deg, #25D366 0%, #128C7E 100%);\n"
    "  border-radius: 14px;\n"
    "  border: none;\n"
    "  color: white;\n"
    "  font-size: 15px;\n"
    "  font-weight: 700;\n"
    "  padding: 12px 32px;\n"
    "  min-height: 22px;\n"
    "  letter-spacing: 0.5px;\n"
    "  transition: all 200ms ease;\n"
    "  box-shadow: 0 4px 15px alpha(#25D366, 0.3);\n"
    "}\n"
    "\n"
    "#login-btn:hover {\n"
    "  background: linear-gradient(135deg, #2ee675 0%, #1aab97 100%);\n"
    "  box-shadow: 0 6px 25px alpha(#25D366, 0.45);\n"
    "}\n"
    "\n"
    "#send-btn {\n"
    "  background: linear-gradient(135deg, #25D366 0%, #128C7E 100%);\n"
    "  border-radius: 50px;\n"
    "  border: none;\n"
    "  color: white;\n"
    "  font-size: 18px;\n"
    "  font-weight: 700;\n"
    "  padding: 8px 18px;\n"
    "  min-width: 48px;\n"
    "  min-height: 48px;\n"
    "  box-shadow: 0 3px 12px alpha(#25D366, 0.35);\n"
    "  transition: all 200ms ease;\n"
    "}\n"
    "\n"
    "#send-btn:hover {\n"
    "  background: linear-gradient(135deg, #2ee675 0%, #1aab97 100%);\n"
    "  box-shadow: 0 5px 20px alpha(#25D366, 0.5);\n"
    "}\n"
    "\n"
    "/* ── Chat Area ──────────────────────────────────────── */\n"
    "#chat-header {\n"
    "  background: alpha(#1a2e35, 0.75);\n"
    "  border-bottom: 1px solid alpha(#25D366, 0.12);\n"
    "  padding: 14px 24px;\n"
    "  border-radius: 0;\n"
    "  box-shadow: 0 2px 10px alpha(black, 0.2);\n"
    "}\n"
    "\n"
    "#chat-header-label {\n"
    "  color: #e9edef;\n"
    "  font-size: 18px;\n"
    "  font-weight: 700;\n"
    "}\n"
    "\n"
    "#chat-status-label {\n"
    "  color: #25D366;\n"
    "  font-size: 12px;\n"
    "}\n"
    "\n"
    "#chat-scroll {\n"
    "  background: transparent;\n"
    "}\n"
    "\n"
    "#chat-viewport {\n"
    "  background: transparent;\n"
    "}\n"
    "\n"
    "#chat-box {\n"
    "  background: transparent;\n"
    "  padding: 16px;\n"
    "}\n"
    "\n"
    "/* ── Message Bubbles ──────────────────────────────── */\n"
    "#msg-sent {\n"
    "  background: linear-gradient(135deg, alpha(#005c4b, 0.85) 0%, "
    "alpha(#00443a, 0.85) 100%);\n"
    "  border-radius: 16px 4px 16px 16px;\n"
    "  padding: 10px 16px;\n"
    "  margin: 3px 8px 3px 80px;\n"
    "  color: #e9edef;\n"
    "  font-size: 14px;\n"
    "  border: 1px solid alpha(#25D366, 0.1);\n"
    "  box-shadow: 0 2px 8px alpha(black, 0.2);\n"
    "}\n"
    "\n"
    "#msg-received {\n"
    "  background: linear-gradient(135deg, alpha(#1a2e35, 0.85) 0%, "
    "alpha(#15252d, 0.85) 100%);\n"
    "  border-radius: 4px 16px 16px 16px;\n"
    "  padding: 10px 16px;\n"
    "  margin: 3px 80px 3px 8px;\n"
    "  color: #e9edef;\n"
    "  font-size: 14px;\n"
    "  border: 1px solid alpha(#25D366, 0.08);\n"
    "  box-shadow: 0 2px 8px alpha(black, 0.2);\n"
    "}\n"
    "\n"
    "#msg-sender-name {\n"
    "  color: #25D366;\n"
    "  font-size: 12px;\n"
    "  font-weight: 700;\n"
    "  margin-bottom: 2px;\n"
    "}\n"
    "\n"
    "#msg-time {\n"
    "  color: alpha(#8696a0, 0.7);\n"
    "  font-size: 10px;\n"
    "  margin-top: 4px;\n"
    "}\n"
    "\n"
    "#msg-content {\n"
    "  color: #e9edef;\n"
    "  font-size: 14px;\n"
    "}\n"
    "\n"
    "/* ── Sidebar ────────────────────────────────────────── */\n"
    "#sidebar {\n"
    "  background: alpha(#111b21, 0.8);\n"
    "  border-right: 1px solid alpha(#25D366, 0.1);\n"
    "  padding: 16px 12px;\n"
    "  min-width: 220px;\n"
    "}\n"
    "\n"
    "#sidebar-title {\n"
    "  color: #e9edef;\n"
    "  font-size: 16px;\n"
    "  font-weight: 700;\n"
    "  margin-bottom: 8px;\n"
    "  padding: 8px 8px;\n"
    "}\n"
    "\n"
    "#sidebar-campus-item {\n"
    "  background: alpha(#1a2e35, 0.5);\n"
    "  border-radius: 12px;\n"
    "  padding: 10px 14px;\n"
    "  margin: 4px 0;\n"
    "  border: 1px solid alpha(#25D366, 0.06);\n"
    "  transition: all 200ms ease;\n"
    "}\n"
    "\n"
    "#sidebar-campus-item:hover {\n"
    "  background: alpha(#1a2e35, 0.8);\n"
    "  border-color: alpha(#25D366, 0.2);\n"
    "}\n"
    "\n"
    "#sidebar-campus-name {\n"
    "  color: #e9edef;\n"
    "  font-size: 14px;\n"
    "  font-weight: 600;\n"
    "}\n"
    "\n"
    "#sidebar-campus-status {\n"
    "  color: #25D366;\n"
    "  font-size: 11px;\n"
    "}\n"
    "\n"
    "#sidebar-dot-online {\n"
    "  color: #25D366;\n"
    "  font-size: 10px;\n"
    "}\n"
    "\n"
    "/* ── Compose Bar ────────────────────────────────────── */\n"
    "#compose-bar {\n"
    "  background: alpha(#1a2e35, 0.75);\n"
    "  border-top: 1px solid alpha(#25D366, 0.1);\n"
    "  padding: 10px 16px;\n"
    "  box-shadow: 0 -2px 10px alpha(black, 0.15);\n"
    "}\n"
    "\n"
    "#compose-selectors {\n"
    "  background: alpha(#0b1a1f, 0.5);\n"
    "  border-radius: 10px;\n"
    "  padding: 8px 12px;\n"
    "  margin-bottom: 8px;\n"
    "  border: 1px solid alpha(#25D366, 0.08);\n"
    "}\n"
    "\n"
    "#compose-selectors label {\n"
    "  color: #8696a0;\n"
    "  font-size: 11px;\n"
    "  font-weight: 600;\n"
    "  letter-spacing: 0.5px;\n"
    "}\n"
    "\n"
    "#msg-entry {\n"
    "  background: alpha(#0b1a1f, 0.7);\n"
    "  border: 1px solid alpha(#25D366, 0.15);\n"
    "  border-radius: 24px;\n"
    "  color: #e9edef;\n"
    "  padding: 10px 20px;\n"
    "  font-size: 14px;\n"
    "  min-height: 22px;\n"
    "  caret-color: #25D366;\n"
    "}\n"
    "\n"
    "#msg-entry:focus {\n"
    "  border-color: alpha(#25D366, 0.4);\n"
    "  box-shadow: 0 0 0 2px alpha(#25D366, 0.1);\n"
    "}\n"
    "\n"
    "/* ── Broadcast Banner ──────────────────────────────── */\n"
    "#broadcast-banner {\n"
    "  background: linear-gradient(90deg, alpha(#128C7E, 0.3) 0%, "
    "alpha(#25D366, 0.15) 100%);\n"
    "  border-radius: 10px;\n"
    "  padding: 8px 16px;\n"
    "  margin: 6px 16px;\n"
    "  border: 1px solid alpha(#25D366, 0.15);\n"
    "}\n"
    "\n"
    "#broadcast-text {\n"
    "  color: #aebac1;\n"
    "  font-size: 12px;\n"
    "}\n"
    "\n"
    "/* ── Scrollbar Styling ─────────────────────────────── */\n"
    "scrollbar {\n"
    "  background: transparent;\n"
    "}\n"
    "\n"
    "scrollbar slider {\n"
    "  background: alpha(#25D366, 0.25);\n"
    "  border-radius: 10px;\n"
    "  min-width: 6px;\n"
    "  min-height: 30px;\n"
    "}\n"
    "\n"
    "scrollbar slider:hover {\n"
    "  background: alpha(#25D366, 0.45);\n"
    "}\n"
    "\n"
    "/* ── Separator ──────────────────────────────────────── */\n"
    "#sep-line {\n"
    "  background: alpha(#25D366, 0.08);\n"
    "  min-height: 1px;\n"
    "}\n"
    "\n"
    "/* ── Logo icon circle ─────────────────────────────── */\n"
    "#logo-circle {\n"
    "  background: linear-gradient(135deg, #25D366 0%, #128C7E 100%);\n"
    "  border-radius: 50px;\n"
    "  min-width: 72px;\n"
    "  min-height: 72px;\n"
    "  padding: 0;\n"
    "  box-shadow: 0 4px 20px alpha(#25D366, 0.35);\n"
    "}\n"
    "\n"
    "#logo-icon {\n"
    "  color: white;\n"
    "  font-size: 36px;\n"
    "}\n"
    "\n"
    "#field-label {\n"
    "  color: #8696a0;\n"
    "  font-size: 12px;\n"
    "  font-weight: 600;\n"
    "  letter-spacing: 0.5px;\n"
    "  margin-bottom: 4px;\n"
    "  margin-top: 10px;\n"
    "}\n"
    "\n"
    "/* ── Welcome overlay ──────────────────────────────── */\n"
    "#welcome-msg {\n"
    "  color: alpha(#8696a0, 0.5);\n"
    "  font-size: 14px;\n"
    "}\n";

/* ─────────────────────────────────────────────────────── */
/*                    HELPER: SCROLL TO BOTTOM             */
/* ─────────────────────────────────────────────────────── */
static gboolean scroll_to_bottom(gpointer data) {
  GtkAdjustment *adj =
      gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(chat_scroll));
  gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj) -
                                    gtk_adjustment_get_page_size(adj));
  return FALSE;
}

/* ─────────────────────────────────────────────────────── */
/*               ADD MESSAGE BUBBLE TO CHAT                */
/* ─────────────────────────────────────────────────────── */
/*
 * Dynamically creates a styled message bubble widget (sent/received)
 * and appends it to the chat container.
 */
static gboolean add_message_bubble(gpointer data) {
  Message *m = (Message *)data;

  /* Outer alignment */
  GtkWidget *align = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

  /* Bubble frame */
  GtkWidget *bubble = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_name(bubble, m->is_sent ? "msg-sent" : "msg-received");
  gtk_widget_set_halign(bubble, m->is_sent ? GTK_ALIGN_END : GTK_ALIGN_START);
  gtk_widget_set_hexpand(bubble, FALSE);
  gtk_widget_set_size_request(bubble, -1, -1);

  /* Sender label */
  char sender_text[120];
  if (m->is_sent)
    snprintf(sender_text, sizeof(sender_text), "You · %s", m->from_dept);
  else
    snprintf(sender_text, sizeof(sender_text), "%s · %s", m->from_campus,
             m->from_dept);

  GtkWidget *sender_lbl = gtk_label_new(sender_text);
  gtk_widget_set_name(sender_lbl, "msg-sender-name");
  gtk_label_set_xalign(GTK_LABEL(sender_lbl), 0.0);
  gtk_box_pack_start(GTK_BOX(bubble), sender_lbl, FALSE, FALSE, 0);

  /* Content */
  GtkWidget *content_lbl = gtk_label_new(m->content);
  gtk_widget_set_name(content_lbl, "msg-content");
  gtk_label_set_xalign(GTK_LABEL(content_lbl), 0.0);
  gtk_label_set_line_wrap(GTK_LABEL(content_lbl), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(content_lbl), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_max_width_chars(GTK_LABEL(content_lbl), 50);
  gtk_box_pack_start(GTK_BOX(bubble), content_lbl, FALSE, FALSE, 0);

  /* Timestamp */
  char time_str[30];
  struct tm *tm_info = localtime(&m->timestamp);
  strftime(time_str, sizeof(time_str), "%H:%M", tm_info);

  char time_display[160];
  snprintf(time_display, sizeof(time_display), "%s  →  %s · %s", time_str,
           m->to_campus, m->to_dept);

  GtkWidget *time_lbl = gtk_label_new(time_display);
  gtk_widget_set_name(time_lbl, "msg-time");
  gtk_label_set_xalign(GTK_LABEL(time_lbl), 1.0);
  gtk_box_pack_start(GTK_BOX(bubble), time_lbl, FALSE, FALSE, 0);

  /* Pack bubble with alignment */
  if (m->is_sent) {
    gtk_box_pack_end(GTK_BOX(align), bubble, FALSE, FALSE, 0);
  } else {
    gtk_box_pack_start(GTK_BOX(align), bubble, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(chat_box), align, FALSE, FALSE, 4);
  gtk_widget_show_all(align);

  /* Scroll after layout */
  g_timeout_add(50, scroll_to_bottom, NULL);

  free(m);
  return FALSE;
}

/* ─────────────────────────────────────────────────────── */
/*             ADD BROADCAST NOTIFICATION                  */
/* ─────────────────────────────────────────────────────── */
static gboolean show_broadcast(gpointer data) {
  char *text = (char *)data;
  gtk_label_set_text(GTK_LABEL(broadcast_label), text);
  gtk_widget_show(gtk_widget_get_parent(broadcast_label));
  free(text);
  return FALSE;
}

/* ─────────────────────────────────────────────────────── */
/*              NETWORK: TCP RECEIVER THREAD               */
/* ─────────────────────────────────────────────────────── */
static void *tcp_receiver(void *arg) {
  char buffer[BUFFER_SIZE];
  while (running) {
    memset(buffer, 0, BUFFER_SIZE);
    int recv_len = recv(tcp_socket_fd, buffer, BUFFER_SIZE - 1, 0);
    if (recv_len <= 0) {
      running = 0;
      break;
    }
    buffer[recv_len] = '\0';

    if (strncmp(buffer, "MSG:", 4) == 0) {
      char temp[BUFFER_SIZE];
      strncpy(temp, buffer, BUFFER_SIZE);

      char *source_campus = strtok(temp + 4, ":");
      char *source_dept = strtok(NULL, ":");
      char *target_campus = strtok(NULL, ":");
      char *target_dept = strtok(NULL, ":");
      char *content = strtok(NULL, "");

      if (source_campus && source_dept && target_campus && target_dept &&
          content) {
        /* Store in history */
        pthread_mutex_lock(&msg_mutex);
        if (msg_count < MAX_MESSAGES) {
          strncpy(messages[msg_count].from_campus, source_campus, 49);
          strncpy(messages[msg_count].from_dept, source_dept, 49);
          strncpy(messages[msg_count].to_campus, target_campus, 49);
          strncpy(messages[msg_count].to_dept, target_dept, 49);
          strncpy(messages[msg_count].content, content, 499);
          messages[msg_count].timestamp = time(NULL);
          messages[msg_count].is_sent = 0;
          msg_count++;
        }
        pthread_mutex_unlock(&msg_mutex);

        /* Create bubble on GUI thread */
        Message *m = malloc(sizeof(Message));
        strncpy(m->from_campus, source_campus, 49);
        strncpy(m->from_dept, source_dept, 49);
        strncpy(m->to_campus, target_campus, 49);
        strncpy(m->to_dept, target_dept, 49);
        strncpy(m->content, content, 499);
        m->timestamp = time(NULL);
        m->is_sent = 0;
        g_idle_add(add_message_bubble, m);
      }
    } else if (strncmp(buffer, "ERROR:", 6) == 0) {
      /* Show error as a received system message */
      Message *m = calloc(1, sizeof(Message));
      strncpy(m->from_campus, "⚠ System", 49);
      strncpy(m->from_dept, "Error", 49);
      strncpy(m->to_campus, campus_name, 49);
      strncpy(m->to_dept, "", 49);
      strncpy(m->content, buffer + 6, 499);
      m->timestamp = time(NULL);
      m->is_sent = 0;
      g_idle_add(add_message_bubble, m);
    }
  }
  return NULL;
}

/* ─────────────────────────────────────────────────────── */
/*             NETWORK: UDP HEARTBEAT SENDER               */
/* ─────────────────────────────────────────────────────── */
static void *udp_heartbeat_sender(void *arg) {
  struct sockaddr_in server_addr;
  char heartbeat_msg[BUFFER_SIZE];
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(UDP_PORT);
  inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
  snprintf(heartbeat_msg, BUFFER_SIZE, "HEARTBEAT:%s", campus_name);
  while (running) {
    sendto(udp_socket_fd, heartbeat_msg, strlen(heartbeat_msg), 0,
           (struct sockaddr *)&server_addr, sizeof(server_addr));
    sleep(HEARTBEAT_INTERVAL);
  }
  return NULL;
}

/* ─────────────────────────────────────────────────────── */
/*            NETWORK: UDP BROADCAST RECEIVER              */
/* ─────────────────────────────────────────────────────── */
static void *udp_broadcast_receiver(void *arg) {
  char buffer[BUFFER_SIZE];
  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);
  while (running) {
    memset(buffer, 0, BUFFER_SIZE);
    int recv_len = recvfrom(udp_socket_fd, buffer, BUFFER_SIZE - 1, 0,
                            (struct sockaddr *)&sender_addr, &addr_len);
    if (recv_len > 0) {
      buffer[recv_len] = '\0';
      if (strncmp(buffer, "BROADCAST:", 10) == 0) {
        char *text = strdup(buffer + 10);
        g_idle_add(show_broadcast, text);
      }
    }
  }
  return NULL;
}

/* ─────────────────────────────────────────────────────── */
/*               GUI: SEND MESSAGE CALLBACK                */
/* ─────────────────────────────────────────────────────── */
/*
 * Callback for the send button; extracts UI input,
 * formats the protocol message, and sends it via TCP.
 */
static void on_send_clicked(GtkWidget *widget, gpointer data) {
  const char *msg_text = gtk_entry_get_text(GTK_ENTRY(msg_entry));
  if (strlen(msg_text) == 0)
    return;

  const char *target = gtk_combo_box_text_get_active_text(
      GTK_COMBO_BOX_TEXT(target_campus_combo));
  int src_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(source_dept_combo));
  int tgt_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(target_dept_combo));

  if (!target || src_idx < 0 || tgt_idx < 0)
    return;

  const char *src_dept = departments[src_idx];
  const char *tgt_dept = departments[tgt_idx];

  /* Send over TCP */
  char full_msg[BUFFER_SIZE];
  snprintf(full_msg, BUFFER_SIZE, "MSG:%s:%s:%s:%s:%s", campus_name, src_dept,
           target, tgt_dept, msg_text);
  send(tcp_socket_fd, full_msg, strlen(full_msg), 0);

  /* Store locally & display */
  pthread_mutex_lock(&msg_mutex);
  if (msg_count < MAX_MESSAGES) {
    strncpy(messages[msg_count].from_campus, campus_name, 49);
    strncpy(messages[msg_count].from_dept, src_dept, 49);
    strncpy(messages[msg_count].to_campus, target, 49);
    strncpy(messages[msg_count].to_dept, tgt_dept, 49);
    strncpy(messages[msg_count].content, msg_text, 499);
    messages[msg_count].timestamp = time(NULL);
    messages[msg_count].is_sent = 1;
    msg_count++;
  }
  pthread_mutex_unlock(&msg_mutex);

  Message *m = malloc(sizeof(Message));
  strncpy(m->from_campus, campus_name, 49);
  strncpy(m->from_dept, src_dept, 49);
  strncpy(m->to_campus, target, 49);
  strncpy(m->to_dept, tgt_dept, 49);
  strncpy(m->content, msg_text, 499);
  m->timestamp = time(NULL);
  m->is_sent = 1;
  g_idle_add(add_message_bubble, m);

  gtk_entry_set_text(GTK_ENTRY(msg_entry), "");
}

/* Enter key sends message */
static void on_msg_entry_activate(GtkWidget *widget, gpointer data) {
  on_send_clicked(NULL, NULL);
}

/* ─────────────────────────────────────────────────────── */
/*                 GUI: LOGIN CALLBACK                     */
/* ─────────────────────────────────────────────────────── */
static void on_login_clicked(GtkWidget *widget, gpointer data) {
  const char *selected =
      gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(campus_combo));
  const char *pwd = gtk_entry_get_text(GTK_ENTRY(password_entry));

  if (!selected || strlen(pwd) == 0) {
    gtk_label_set_text(GTK_LABEL(login_status_label),
                       "⚠  Please select a campus and enter password");
    return;
  }

  strncpy(campus_name, selected, 49);
  strncpy(campus_password, pwd, 49);

  /* Show spinner */
  gtk_spinner_start(GTK_SPINNER(login_spinner));
  gtk_label_set_text(GTK_LABEL(login_status_label), "");
  gtk_widget_set_sensitive(login_btn, FALSE);

  /* TCP connect */
  struct sockaddr_in server_addr;
  tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (tcp_socket_fd < 0) {
    gtk_label_set_text(GTK_LABEL(login_status_label),
                       "⚠  TCP socket creation failed");
    gtk_spinner_stop(GTK_SPINNER(login_spinner));
    gtk_widget_set_sensitive(login_btn, TRUE);
    return;
  }
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(TCP_PORT);
  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    gtk_label_set_text(GTK_LABEL(login_status_label),
                       "⚠  Invalid server address");
    gtk_spinner_stop(GTK_SPINNER(login_spinner));
    gtk_widget_set_sensitive(login_btn, TRUE);
    close(tcp_socket_fd);
    tcp_socket_fd = -1;
    return;
  }
  if (connect(tcp_socket_fd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    gtk_label_set_text(GTK_LABEL(login_status_label),
                       "⚠  Cannot connect – is the server running?");
    gtk_spinner_stop(GTK_SPINNER(login_spinner));
    gtk_widget_set_sensitive(login_btn, TRUE);
    close(tcp_socket_fd);
    tcp_socket_fd = -1;
    return;
  }

  /* Authenticate */
  char auth_msg[BUFFER_SIZE];
  snprintf(auth_msg, BUFFER_SIZE, "AUTH:%s:%s", campus_name, campus_password);
  send(tcp_socket_fd, auth_msg, strlen(auth_msg), 0);

  char buffer[BUFFER_SIZE];
  int recv_len = recv(tcp_socket_fd, buffer, BUFFER_SIZE - 1, 0);
  if (recv_len > 0) {
    buffer[recv_len] = '\0';
    if (strcmp(buffer, "AUTH_OK") != 0) {
      gtk_label_set_text(GTK_LABEL(login_status_label),
                         "⚠  Authentication failed – wrong password");
      gtk_spinner_stop(GTK_SPINNER(login_spinner));
      gtk_widget_set_sensitive(login_btn, TRUE);
      close(tcp_socket_fd);
      tcp_socket_fd = -1;
      return;
    }
  } else {
    gtk_label_set_text(GTK_LABEL(login_status_label),
                       "⚠  No response from server");
    gtk_spinner_stop(GTK_SPINNER(login_spinner));
    gtk_widget_set_sensitive(login_btn, TRUE);
    close(tcp_socket_fd);
    tcp_socket_fd = -1;
    return;
  }

  /* UDP socket */
  udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in udp_addr;
  memset(&udp_addr, 0, sizeof(udp_addr));
  udp_addr.sin_family = AF_INET;
  udp_addr.sin_addr.s_addr = INADDR_ANY;
  udp_addr.sin_port = 0;
  bind(udp_socket_fd, (struct sockaddr *)&udp_addr, sizeof(udp_addr));

  /* Success! */
  authenticated = 1;
  running = 1;

  /* Update header */
  char hdr[120];
  snprintf(hdr, sizeof(hdr), "🏫  %s Campus", campus_name);
  gtk_label_set_text(GTK_LABEL(header_label), hdr);
  gtk_label_set_text(GTK_LABEL(status_label),
                     "● Online – Connected to Central Server");

  /* Start network threads */
  pthread_t t1, t2, t3;
  pthread_create(&t1, NULL, tcp_receiver, NULL);
  pthread_create(&t2, NULL, udp_heartbeat_sender, NULL);
  pthread_create(&t3, NULL, udp_broadcast_receiver, NULL);
  pthread_detach(t1);
  pthread_detach(t2);
  pthread_detach(t3);

  gtk_spinner_stop(GTK_SPINNER(login_spinner));

  /* Switch to chat page */
  gtk_stack_set_visible_child(GTK_STACK(login_stack), chat_page);
}

/* ─────────────────────────────────────────────────────── */
/*                GUI: BUILD LOGIN PAGE                    */
/* ─────────────────────────────────────────────────────── */
static GtkWidget *build_login_page(void) {
  GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name(page, "login-page");
  gtk_widget_set_vexpand(page, TRUE);
  gtk_widget_set_hexpand(page, TRUE);

  /* Center container */
  GtkWidget *center = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(center, GTK_ALIGN_CENTER);

  /* Card */
  GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_name(card, "login-card");
  gtk_widget_set_size_request(card, 420, -1);

  /* Logo circle */
  GtkWidget *logo_frame = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign(logo_frame, GTK_ALIGN_CENTER);
  GtkWidget *logo_circle = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name(logo_circle, "logo-circle");
  gtk_widget_set_halign(logo_circle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(logo_circle, GTK_ALIGN_CENTER);
  GtkWidget *logo_icon = gtk_label_new("🌐");
  gtk_widget_set_name(logo_icon, "logo-icon");
  gtk_box_pack_start(GTK_BOX(logo_circle), logo_icon, TRUE, TRUE, 12);
  gtk_box_pack_start(GTK_BOX(logo_frame), logo_circle, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), logo_frame, FALSE, FALSE, 8);

  /* Title */
  GtkWidget *title = gtk_label_new("Campus Connect");
  gtk_widget_set_name(title, "login-title");
  gtk_box_pack_start(GTK_BOX(card), title, FALSE, FALSE, 0);

  GtkWidget *subtitle = gtk_label_new("Inter-Campus Messaging System");
  gtk_widget_set_name(subtitle, "login-subtitle");
  gtk_box_pack_start(GTK_BOX(card), subtitle, FALSE, FALSE, 0);

  /* Campus selector */
  GtkWidget *campus_lbl = gtk_label_new("CAMPUS");
  gtk_widget_set_name(campus_lbl, "field-label");
  gtk_label_set_xalign(GTK_LABEL(campus_lbl), 0.0);
  gtk_box_pack_start(GTK_BOX(card), campus_lbl, FALSE, FALSE, 0);

  campus_combo = gtk_combo_box_text_new();
  const char *campuses[] = {"Islamabad", "Lahore", "Karachi",
                            "Peshawar",  "CFD",    "Multan"};
  for (int i = 0; i < 6; i++)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(campus_combo),
                                   campuses[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(campus_combo), 0);
  gtk_box_pack_start(GTK_BOX(card), campus_combo, FALSE, FALSE, 4);

  /* Password */
  GtkWidget *pwd_lbl = gtk_label_new("PASSWORD");
  gtk_widget_set_name(pwd_lbl, "field-label");
  gtk_label_set_xalign(GTK_LABEL(pwd_lbl), 0.0);
  gtk_box_pack_start(GTK_BOX(card), pwd_lbl, FALSE, FALSE, 0);

  password_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry),
                                 "Enter campus password");
  gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
  gtk_entry_set_invisible_char(GTK_ENTRY(password_entry), 0x2022);
  gtk_box_pack_start(GTK_BOX(card), password_entry, FALSE, FALSE, 4);

  /* Login button */
  login_btn = gtk_button_new_with_label("⚡  Connect");
  gtk_widget_set_name(login_btn, "login-btn");
  gtk_widget_set_margin_top(login_btn, 16);
  g_signal_connect(login_btn, "clicked", G_CALLBACK(on_login_clicked), NULL);
  g_signal_connect(password_entry, "activate", G_CALLBACK(on_login_clicked),
                   NULL);
  gtk_box_pack_start(GTK_BOX(card), login_btn, FALSE, FALSE, 0);

  /* Spinner */
  login_spinner = gtk_spinner_new();
  gtk_widget_set_halign(login_spinner, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(card), login_spinner, FALSE, FALSE, 4);

  /* Status label */
  login_status_label = gtk_label_new("");
  gtk_widget_set_name(login_status_label, "login-status");
  gtk_box_pack_start(GTK_BOX(card), login_status_label, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(center), card, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(page), center, TRUE, TRUE, 0);

  return page;
}

/* ─────────────────────────────────────────────────────── */
/*               GUI: BUILD SIDEBAR                        */
/* ─────────────────────────────────────────────────────── */
static GtkWidget *build_sidebar(void) {
  GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name(sidebar, "sidebar");

  GtkWidget *title = gtk_label_new("💬  Campuses");
  gtk_widget_set_name(title, "sidebar-title");
  gtk_label_set_xalign(GTK_LABEL(title), 0.0);
  gtk_box_pack_start(GTK_BOX(sidebar), title, FALSE, FALSE, 0);

  /* Separator */
  GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_name(sep, "sep-line");
  gtk_box_pack_start(GTK_BOX(sidebar), sep, FALSE, FALSE, 8);

  const char *campus_list[] = {"Islamabad", "Lahore", "Karachi",
                               "Peshawar",  "CFD",    "Multan"};
  const char *campus_icons[] = {"🏛", "🕌", "🌊", "⛰", "🏫", "🏜"};
  for (int i = 0; i < 6; i++) {
    GtkWidget *item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_name(item, "sidebar-campus-item");

    /* Icon */
    GtkWidget *icon = gtk_label_new(campus_icons[i]);
    gtk_box_pack_start(GTK_BOX(item), icon, FALSE, FALSE, 0);

    /* Name + status */
    GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name = gtk_label_new(campus_list[i]);
    gtk_widget_set_name(name, "sidebar-campus-name");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0);
    gtk_box_pack_start(GTK_BOX(info), name, FALSE, FALSE, 0);

    GtkWidget *stat = gtk_label_new("● Available");
    gtk_widget_set_name(stat, "sidebar-campus-status");
    gtk_label_set_xalign(GTK_LABEL(stat), 0.0);
    gtk_box_pack_start(GTK_BOX(info), stat, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(item), info, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), item, FALSE, FALSE, 0);
  }

  /* Spacer */
  GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(sidebar), spacer, TRUE, TRUE, 0);

  /* Version */
  GtkWidget *ver = gtk_label_new("v2.0 · CN Lab Project");
  gtk_widget_set_name(ver, "welcome-msg");
  gtk_widget_set_margin_bottom(ver, 8);
  gtk_box_pack_end(GTK_BOX(sidebar), ver, FALSE, FALSE, 0);

  return sidebar;
}

/* ─────────────────────────────────────────────────────── */
/*                GUI: BUILD CHAT PAGE                     */
/* ─────────────────────────────────────────────────────── */
static GtkWidget *build_chat_page(void) {
  GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

  /* ── Sidebar ── */
  GtkWidget *sidebar = build_sidebar();
  gtk_box_pack_start(GTK_BOX(page), sidebar, FALSE, FALSE, 0);

  /* ── Main chat area ── */
  GtkWidget *main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(main_area, TRUE);

  /* Header */
  GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_name(header, "chat-header");

  header_label = gtk_label_new("🏫  Campus");
  gtk_widget_set_name(header_label, "chat-header-label");
  gtk_label_set_xalign(GTK_LABEL(header_label), 0.0);
  gtk_box_pack_start(GTK_BOX(header), header_label, FALSE, FALSE, 0);

  status_label = gtk_label_new("● Online");
  gtk_widget_set_name(status_label, "chat-status-label");
  gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
  gtk_box_pack_start(GTK_BOX(header), status_label, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(main_area), header, FALSE, FALSE, 0);

  /* Broadcast banner (hidden initially) */
  GtkWidget *broadcast_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_name(broadcast_box, "broadcast-banner");
  GtkWidget *bc_icon = gtk_label_new("📢");
  gtk_box_pack_start(GTK_BOX(broadcast_box), bc_icon, FALSE, FALSE, 0);
  broadcast_label = gtk_label_new("");
  gtk_widget_set_name(broadcast_label, "broadcast-text");
  gtk_label_set_line_wrap(GTK_LABEL(broadcast_label), TRUE);
  gtk_box_pack_start(GTK_BOX(broadcast_box), broadcast_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(main_area), broadcast_box, FALSE, FALSE, 0);
  gtk_widget_set_no_show_all(broadcast_box, TRUE);

  /* Chat scroll area */
  chat_scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_name(chat_scroll, "chat-scroll");
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chat_scroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(chat_scroll, TRUE);

  /* Viewport transparency */
  GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
  gtk_widget_set_name(viewport, "chat-viewport");

  chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_name(chat_box, "chat-box");

  /* Welcome message */
  GtkWidget *welcome =
      gtk_label_new("🔒  Messages are end-to-end encrypted\nStart a "
                    "conversation by selecting a target campus below");
  gtk_widget_set_name(welcome, "welcome-msg");
  gtk_widget_set_valign(welcome, GTK_ALIGN_CENTER);
  gtk_widget_set_vexpand(welcome, TRUE);
  gtk_label_set_justify(GTK_LABEL(welcome), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start(GTK_BOX(chat_box), welcome, TRUE, TRUE, 40);

  gtk_container_add(GTK_CONTAINER(viewport), chat_box);
  gtk_container_add(GTK_CONTAINER(chat_scroll), viewport);
  gtk_box_pack_start(GTK_BOX(main_area), chat_scroll, TRUE, TRUE, 0);

  /* ── Compose bar ── */
  GtkWidget *compose = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_name(compose, "compose-bar");

  /* Selector row */
  GtkWidget *selectors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_name(selectors, "compose-selectors");

  /* Target campus */
  GtkWidget *tc_lbl = gtk_label_new("TO CAMPUS");
  gtk_box_pack_start(GTK_BOX(selectors), tc_lbl, FALSE, FALSE, 0);
  target_campus_combo = gtk_combo_box_text_new();
  const char *campuses[] = {"Islamabad", "Lahore", "Karachi",
                            "Peshawar",  "CFD",    "Multan"};
  for (int i = 0; i < 6; i++)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(target_campus_combo),
                                   campuses[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(target_campus_combo), 0);
  gtk_box_pack_start(GTK_BOX(selectors), target_campus_combo, FALSE, FALSE, 0);

  /* Source dept */
  GtkWidget *sd_lbl = gtk_label_new("FROM DEPT");
  gtk_box_pack_start(GTK_BOX(selectors), sd_lbl, FALSE, FALSE, 4);
  source_dept_combo = gtk_combo_box_text_new();
  for (int i = 0; i < num_departments; i++)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(source_dept_combo),
                                   departments[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(source_dept_combo), 0);
  gtk_box_pack_start(GTK_BOX(selectors), source_dept_combo, FALSE, FALSE, 0);

  /* Target dept */
  GtkWidget *td_lbl = gtk_label_new("TO DEPT");
  gtk_box_pack_start(GTK_BOX(selectors), td_lbl, FALSE, FALSE, 4);
  target_dept_combo = gtk_combo_box_text_new();
  for (int i = 0; i < num_departments; i++)
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(target_dept_combo),
                                   departments[i]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(target_dept_combo), 0);
  gtk_box_pack_start(GTK_BOX(selectors), target_dept_combo, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(compose), selectors, FALSE, FALSE, 0);

  /* Message input row */
  GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

  msg_entry = gtk_entry_new();
  gtk_widget_set_name(msg_entry, "msg-entry");
  gtk_entry_set_placeholder_text(GTK_ENTRY(msg_entry), "Type a message...");
  gtk_widget_set_hexpand(msg_entry, TRUE);
  g_signal_connect(msg_entry, "activate", G_CALLBACK(on_msg_entry_activate),
                   NULL);
  gtk_box_pack_start(GTK_BOX(input_row), msg_entry, TRUE, TRUE, 0);

  send_btn = gtk_button_new_with_label("➤");
  gtk_widget_set_name(send_btn, "send-btn");
  g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), NULL);
  gtk_box_pack_end(GTK_BOX(input_row), send_btn, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(compose), input_row, FALSE, FALSE, 0);

  gtk_box_pack_end(GTK_BOX(main_area), compose, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(page), main_area, TRUE, TRUE, 0);
  return page;
}

/* ─────────────────────────────────────────────────────── */
/*                   WINDOW CLOSE HANDLER                  */
/* ─────────────────────────────────────────────────────── */
static void on_window_destroy(GtkWidget *widget, gpointer data) {
  running = 0;
  if (tcp_socket_fd >= 0)
    close(tcp_socket_fd);
  if (udp_socket_fd >= 0)
    close(udp_socket_fd);
  gtk_main_quit();
}

/* ─────────────────────────────────────────────────────── */
/*                          MAIN                           */
/* ─────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  /* ── Apply CSS ── */
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, css_data, -1, NULL);
  gtk_style_context_add_provider_for_screen(
      gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);

  /* ── Main window ── */
  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(main_window),
                       "Campus Connect – Inter-Campus Messaging");
  gtk_window_set_default_size(GTK_WINDOW(main_window), 1100, 720);
  gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
  g_signal_connect(main_window, "destroy", G_CALLBACK(on_window_destroy), NULL);

  /* ── Stack for login / chat views ── */
  login_stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(login_stack),
                                GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration(GTK_STACK(login_stack), 400);

  login_page = build_login_page();
  chat_page = build_chat_page();

  gtk_stack_add_named(GTK_STACK(login_stack), login_page, "login");
  gtk_stack_add_named(GTK_STACK(login_stack), chat_page, "chat");
  gtk_stack_set_visible_child(GTK_STACK(login_stack), login_page);

  gtk_container_add(GTK_CONTAINER(main_window), login_stack);
  gtk_widget_show_all(main_window);

  gtk_main();
  return 0;
}
