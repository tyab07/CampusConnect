
# Campus Connect – Inter-Campus Messaging System

A high-performance inter-campus networking system with a flagship GTK3-based GUI client.

## 🌟 Features
- **Glassy WhatsApp UI**: Modern dark theme with glassmorphism effects.
- **Micro-Messaging**: TCP-based departmental communication.
- **Heartbeat System**: Real-time campus presence tracking via UDP.
- **Broadcast System**: System-wide notifications from the central server.

## 🛠️ Build & Run
### Central Server
```bash
gcc central_server2.c -o server -lpthread
./server
```

### GUI Client
```bash
gcc campus_client_gui.c -o campus_client_gui $(pkg-config --cflags --libs gtk+-3.0) -lpthread
./campus_client_gui
```

## 📝 Author
CN Lab Project Team
