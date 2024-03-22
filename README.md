# OfflineMessenger application using TCP Sockets in C language

The project aims to develop a client/server application facilitating message exchange among connected users while also enabling message delivery to offline users, with messages appearing upon their server connection. Additionally, users will have the capability to reply specifically to certain received messages. The application will also provide conversation history for each user individually.

# Configurig the Application
Download the following files:
1. server.c
2. client.c
3. messenger.db
   
Compile the Application:
1. server: ```gcc -o server server.c -lsqlite3```
2. client: ```gcc -Wall client.c -o client```

# How to use:
1. Open two terminal windows.
2. In the first terminal, run the server: ```./server```
3. In the second terminal, run the client, specifying the server IP and port (default 2908): ```./client 127.0.0.1 2908```
4. You cand have as many clients (users of the application) as you want by reapting step 3.
