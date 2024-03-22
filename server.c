#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>

/* portul folosit */
#define PORT 2908
#define RESPONSE_SIZE 1024

sqlite3 *db;

extern int errno;

typedef struct {
  char username[50];
  char password[50];
}User;


typedef struct thDate{
  int IdThread; //id-ul thread-ului tinut in evidenta de acest program
  int cl; //descriptorul intors de accept
  char raspuns[RESPONSE_SIZE];
  int logat; //0-neautentificat, 1-autentificat
  int IdUser;
  int IdDestinatar;
  int deconectareThread;
}thDate;

// struct pentru a verifica daca userul e logat sau nu
typedef struct {
  thDate *td;
  int gasit;
}DateLogin;

// structura pentru a vedea userri online pt functie de trimitere mesaje online online
typedef struct{
  int sock;
  int IdUser;
  char Username[50];
}UserConnection;
UserConnection conexiuniActive[100];
int nrConexiuni = 0;

// struct pentru conversatie
typedef struct {
    int IdUtilizator1;
    int IdUtilizator2;
} Conversatie;

// struct pentru informatiile despre conexiuni
typedef struct {
    Conversatie conversatii[100];
    int numarConversatii;
} ConversatiiInfo;
ConversatiiInfo info;

static void *treat(void *);

/// functie care trimite raspunsurile la client ///
void trimiteRaspuns(int descr_sk, const char *raspuns, int idThread){
  int lungime = strlen(raspuns)+1;

  if(write(descr_sk, &lungime, sizeof(lungime)) <=0){
    printf("Eroare la trimiterea lungimii mesajului.");
    //return errno;
  }
  if(write(descr_sk, raspuns, strlen(raspuns) + 1)<=0){
    printf("Eroare la trimiterea raspunsului.");
  }
  
  printf("Lungimea mesajului primit: %d\n", lungime);
  printf("[Thread %d] Mesajul trimis este: %s\n", idThread, raspuns);
}

/// fucntia de verificare pt login ///
int verificareLogin(void *data, int argc, char **argv, char **aznumeCol){
    DateLogin *dateLogin = (DateLogin*)data;
    if(argc>0){
      dateLogin->gasit = 1;
      dateLogin->td->IdUser = atoi(argv[0]);
    }else{
      dateLogin->gasit = 0;
    }
    return 0;
}

/// functia de verificare mesaje ///
int verificareMesaje(void *data, int argc, char **argv, char **aznumeCol){
  char *raspuns = (char*)data;
  int lungimeCurenta = strlen(raspuns);
  int spatiuRamas = RESPONSE_SIZE - lungimeCurenta - 1;

  for(int i=0; i<argc; i++){
    int lungimeData = strlen(argv[i]);
    if(lungimeData > spatiuRamas){
      lungimeData = spatiuRamas;
    }

    strncat(raspuns, argv[i], lungimeData);
    spatiuRamas -= lungimeData;

    if(spatiuRamas > 0){
      strcat(raspuns, "\n");
      spatiuRamas -= 1;
    }else{
      break;
    }
  }
    return 0;
}

/// functia de inregistrare ///
void inregistrare(thDate *td){
  User new_user;
  snprintf(td->raspuns, RESPONSE_SIZE, "[server]Ati selectat comanda de inregistare...\n");

  // Read username and password from client
  if (read(td->cl, new_user.username, sizeof(new_user.username)) <= 0 || read(td->cl, new_user.password, sizeof(new_user.password)) <= 0) {
      perror("[server] Eroare la citirea usernameului sau a parolei de la client.\n");
      return;
  }

  char sql[256];
  sprintf(sql, "INSERT INTO Utilizatori (Username, Parola) VALUES('%s', '%s');", new_user.username, new_user.password);
  char *err_msg = 0;
  int bd = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if(bd != SQLITE_OK){
    if( bd == SQLITE_CONSTRAINT){
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Username-ul deja exista. Incercati sa va logati.\n");
    }
    else{
      fprintf(stderr, "SQL error: %s\n", err_msg);
      sqlite3_free(err_msg);
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Inregistrarea a esuat. Incercati din nou.\n");
    }
  }else{
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Inregistrare efectuata cu succes! Incercati sa va logati!\n");
    }
  trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

/// functia de login ///
void login(thDate *td){
  User user;
  if(read(td->cl, user.username, sizeof(user.username))<=0 || read(td->cl, user.password, sizeof(user.password))<=0){
    perror("[server] Eroare la citirea username-ului sau a parolei din client\n");
    return;
  }

  DateLogin dateLogin;
  dateLogin.td = td;
  dateLogin.gasit = 0;

  char sql[256];
  sprintf(sql, "SELECT IdUser FROM Utilizatori WHERE Username = '%s' AND Parola='%s';", user.username, user.password);
  char *err_msg = 0;
  
  int bd = sqlite3_exec(db, sql, verificareLogin, &dateLogin, &err_msg);

  if(bd != SQLITE_OK){
    snprintf(td->raspuns, RESPONSE_SIZE, "[server]Logarea a esuat.\n");
  }else{
    if(dateLogin.gasit){
      td->logat = 1;
      td->IdUser = dateLogin.td->IdUser;

      snprintf(td->raspuns, RESPONSE_SIZE, "[server] V-ati logat cu succes! ID utilizator: %d\n", td->IdUser);
      
      char sqlOnline[512];
      sprintf(sqlOnline, "UPDATE Utilizatori SET online = 1 WHERE IdUser = %d;", td->IdUser);
      char *err_msg_online = 0;
      sqlite3_exec(db, sqlOnline, NULL, 0, &err_msg_online);
      if(err_msg_online != NULL){
        fprintf(stderr, "Eroare la actualizarea starii online: %s.\n", err_msg_online);
        sqlite3_free(err_msg_online);
      }

      conexiuniActive[nrConexiuni].sock = td->cl;
      conexiuniActive[nrConexiuni].IdUser = td->IdUser;
      strncpy(conexiuniActive[nrConexiuni].Username, user.username, sizeof(conexiuniActive[nrConexiuni].Username));
      nrConexiuni++;
      
    }else{
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Username sau parola gresita. Incercati din nou!\n");
    }
  }
    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

void afisareUseriOnline(thDate *td){
  
  char buffer[RESPONSE_SIZE] = "";
  int exista = 0;
  for(int i = 0; i < nrConexiuni; i++){
    if(conexiuniActive[i].IdUser != td->IdUser){
      snprintf(buffer+ strlen(buffer), RESPONSE_SIZE - strlen(buffer), "Utilizatorul: %s\n", conexiuniActive[i].Username);
      exista = 1;
    }
  }

  if(!exista){
    snprintf(td->raspuns, RESPONSE_SIZE, "Nu este niciun utilizator online.\n");
  }else{
    snprintf(td->raspuns, RESPONSE_SIZE, "Lista de utilizatori online este: %s", buffer);
  }

  trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}


/// functia care marcheaza mesajele ca citite sau necitite ///
void marcareMesaje(int IdUser, sqlite3 *db){
  char sql[256];
  snprintf(sql, sizeof(sql), "UPDATE Mesaje SET Citit = 1 WHERE IdDestinatar = %d AND Citit = 0;", IdUser);
  char *err_msg = 0;
  int bd = sqlite3_exec(db, sql, 0, 0, &err_msg);
  if(bd != SQLITE_OK){
    fprintf(stderr, "Eroarea la baza de date: %s\n", err_msg);
    sqlite3_free(err_msg);
  }
}

/// functia pentru a vizualiza mesajele noi ///
void newMessages(thDate *td){
  char sql[512];
  sprintf(sql, "SELECT IdMesaj, TextMesaj FROM Mesaje WHERE IdDestinatar = %d AND Citit = 0;", td->IdUser);

  char *err_msg = 0;
  char raspuns[RESPONSE_SIZE] = "";
  int bd = sqlite3_exec(db, sql, verificareMesaje, (void*)raspuns, &err_msg);

  if(bd != SQLITE_OK){
    fprintf(stderr, "Eroarea la baza de date: %s\n", err_msg);
    sqlite3_free(err_msg);
    snprintf(raspuns, RESPONSE_SIZE, "[server] Eroare la preluarea mesajelor noi.\n");
  }else{
    if(strlen(raspuns) == 0){
      snprintf(raspuns, RESPONSE_SIZE, "[server] Nu sunt mesaje noi.\n");
    }
  }
  trimiteRaspuns(td->cl, raspuns, td->IdThread);
  marcareMesaje(td->IdUser, db);
}

// pentru istoric user1-user2
int afisareConversatii(void *data, int argc, char **argv, char **azColName) {
    char* conv = (char*)data;
    char linie[512];

    if (argc >= 2) {
        snprintf(linie, sizeof(linie), "IdMesaj: %s, TextMesaj: %s\n", argv[0], argv[1]);
        if (strlen(conv) + strlen(linie) < RESPONSE_SIZE) {
            strcat(conv, linie); 
        }
    }

    printf("Linia este %s\n", linie); 
    return 0; 
}

/// transformare id in conversatie ///
int preluareIdConversatie(void *data, int argc, char **argv, char **azColName){
    if(argc > 0){
        *(int*)data = atoi(argv[0]);
        return 0;
    }
    return 1;
}

/// functia de reply ///
void reply(thDate *td){
  char numeDestinatar[50];
  char mesaj[256];
  int idMesajRaspuns;
  char sqlMesaje[512];
  char sqlInter2[512];
  char sqlConversatieAct[512];
  char *err_msg=NULL;
  int idUltimMesaj;

  if(read(td->cl, numeDestinatar, sizeof(numeDestinatar)) <= 0){
    perror("[server] Eroare la citirea numelui destinatar.");
    return;
  }

  char conversatie[RESPONSE_SIZE] = "";

  snprintf(sqlMesaje, sizeof(sqlMesaje), "SELECT IdMesaj, TextMesaj, Timestamp FROM Mesaje WHERE (IdExpeditor = %d AND IdDestinatar = (SELECT IdUser FROM Utilizatori WHERE Username = '%s')) OR (IdExpeditor = (SELECT IdUser FROM Utilizatori WHERE Username = '%s') AND IdDestinatar = %d) ORDER BY Timestamp;", td->IdUser, numeDestinatar, numeDestinatar, td->IdUser);
  
  printf("Execut interogarea: %s\n", sqlMesaje);
  sqlite3_exec(db, sqlMesaje, afisareConversatii, conversatie, &err_msg);

  if(err_msg != NULL){
    fprintf(stderr, "Eroare la preluarea mesajelor: %s\n", err_msg);
    sqlite3_free(err_msg);
  }

  printf("Raspunsul este: %s\n", conversatie);

  if(strlen(conversatie) == 0){
    snprintf(td->raspuns, RESPONSE_SIZE, "[server] Nu exista mesaje in conversatia cu %s\n", numeDestinatar);
    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
    return;
  }else{
    snprintf(td->raspuns, RESPONSE_SIZE, "[server] Conversatia cu %s:\n%s", numeDestinatar, conversatie);
    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
  }

  printf("Raspunsul este: %s\n", td->raspuns);

  if(read(td->cl, &idMesajRaspuns, sizeof(int)) <=0 || read(td->cl, mesaj, sizeof(mesaj)) <=0 ){
    perror("[server] Eroare la citirea ID-ului mesajului sau a mesajului de raspuns.");
    return;
  }

  //Inserarea mesajului in baza de date
  snprintf(sqlInter2, sizeof(sqlInter2), "INSERT INTO Mesaje(IdExpeditor, IdDestinatar, TextMesaj, IdRaspunsMesaj) VALUES (%d, (SELECT IdUser FROM Utilizatori WHERE Username = '%s'), '%s', '%d');", td->IdUser, numeDestinatar, mesaj, idMesajRaspuns);
  if(sqlite3_exec(db, sqlInter2, 0, 0, &err_msg) != SQLITE_OK){
    fprintf(stderr, "Eroare la inserarea mesajului de raspuns: %s\n", err_msg);
    sqlite3_free(err_msg);
    snprintf(td->raspuns, RESPONSE_SIZE, "[server] Eroare la trimiterea mesajului de raspuns.\n");
    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
    return;
  }

  idUltimMesaj = sqlite3_last_insert_rowid(db);


  snprintf(sqlConversatieAct, sizeof(sqlConversatieAct), "UPDATE Conversatii SET IdUltimulMesaj = %d WHERE (IdUtilizator1 = %d AND IdUtilizator2 = (SELECT IdUser FROM Utilizatori WHERE Username = '%s')) OR (IdUtilizator1 = (SELECT IdUser FROM Utilizatori WHERE Username = '%s') AND IdUtilizator2 = %d);", idUltimMesaj, td->IdUser, numeDestinatar, numeDestinatar, td->IdUser);
  if(sqlite3_exec(db, sqlConversatieAct, 0, 0, &err_msg) != SQLITE_OK){
    fprintf(stderr, "Eroare la actualizarea tabelului Conversatii: %s\n", err_msg);
    sqlite3_free(err_msg);
  }

  snprintf(td->raspuns, RESPONSE_SIZE, "[server] Mesajul de raspuns a fost trimis cu succes.\n");
  trimiteRaspuns(td->cl, td->raspuns, td->IdThread);

}
/// transformare Utilizator in Id ///
int preluareIdUtilizator(void *data, int argc, char **argv, char **azColName){
  int *IdDestinatar = (int *)data;

  if(argc > 0 && argv[0]){
    *(int*)data = atoi(argv[0]);
    return 0;
  }
  return 1;
}

/// functia care trimite mesaje noi /// 
void sendNewMessages(thDate *td){
    
    char mesaj[256];
    char numeDestinatar[50];
    int IdDestinatar = -1;

    // Citirea ID-ului destinatarului și a mesajului din client
    if(read(td->cl, numeDestinatar, sizeof(numeDestinatar)) <= 0 ||  read(td->cl, mesaj, sizeof(mesaj)) <=0 ){
      perror("[server] Eroare la citirea ID-ului destinatarului sau a mesajului din client.");
      return;
    }

    printf("Mesajul primit este: %s\n", mesaj);
    char sqlQuery[512];
    sprintf(sqlQuery, "SELECT IdUser FROM Utilizatori WHERE Username = '%s';", numeDestinatar);

    //obtinerea Idului
    char *err_msg1 = NULL;
    int bd1 = sqlite3_exec(db, sqlQuery, preluareIdUtilizator, &IdDestinatar, &err_msg1);
    if( bd1 != SQLITE_OK || IdDestinatar == -1){
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Eroare, destinatarul nu exista.\n");
      trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
      if(err_msg1 != NULL){
        sqlite3_free(err_msg1);
      }
      return;
    } 

    // Inserarea mesajului în tabelul Mesaje
    char sqlInsertMesaj[512];
    sprintf(sqlInsertMesaj, "INSERT INTO Mesaje (IdExpeditor, IdDestinatar, TextMesaj) VALUES(%d, %d, '%s');", td->IdUser, IdDestinatar, mesaj);
    char *err_msg = 0;
    int bd = sqlite3_exec(db, sqlInsertMesaj, 0, 0, &err_msg);

    if(bd != SQLITE_OK){
        fprintf(stderr, "Eroare la baza de date: %s\n", err_msg);
        sqlite3_free(err_msg);
        snprintf(td->raspuns, RESPONSE_SIZE, "[server] Trimiterea mesajului a esuat.\n");
        trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
        return;
    }
    printf("[server] ID-ul destinatarului dupa interogare: %d\n", IdDestinatar);

    // obținerea ID-ului ultimului mesaj inserat
    int idMesajNou = sqlite3_last_insert_rowid(db);

    // verificarea dacă există deja o conversație între expeditor și destinatar
    char sqlVerificare[512];
    int idConversatie=0;
    sprintf(sqlVerificare, "SELECT IdConversatie FROM Conversatii WHERE (IdUtilizator1 = %d AND IdUtilizator2 = %d) OR (IdUtilizator1 = %d AND IdUtilizator2 = %d);", td->IdUser, IdDestinatar, IdDestinatar, td->IdUser);
    bd = sqlite3_exec(db, sqlVerificare, preluareIdConversatie, &idConversatie, &err_msg);
    
    // actualizarea IdUltimulMesaj daca este cazul
    if(idConversatie > 0){
        char sqlUpdate[512];
        sprintf(sqlUpdate, "UPDATE Conversatii SET IdUltimulMesaj = %d WHERE IdConversatie = %d;", idMesajNou, idConversatie);
        sqlite3_exec(db, sqlUpdate, 0, 0, &err_msg);
    } else {
        // Creează o nouă înregistrare în tabelul Conversatii
        char sqlInsert[512];
        sprintf(sqlInsert, "INSERT INTO Conversatii (IdUtilizator1, IdUtilizator2, IdUltimulMesaj) VALUES (%d, %d, %d);", td->IdUser, IdDestinatar, idMesajNou);
        sqlite3_exec(db, sqlInsert, 0, 0, &err_msg);
    }

    snprintf(td->raspuns, RESPONSE_SIZE, "[server] Mesajul a fost trimis cu succes.\n");
    printf("%s", td->raspuns);
    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

// functia pentru preluarea conversatiilor pentru istoric //
int preluareConversatii(void *data, int argc, char **argv, char **azColName) {
    ConversatiiInfo *info = (ConversatiiInfo *)data;

    if (info->numarConversatii < 100) {
        info->conversatii[info->numarConversatii].IdUtilizator1 = atoi(argv[0]);
        info->conversatii[info->numarConversatii].IdUtilizator2 = atoi(argv[1]);
        info->numarConversatii++;
    }

    return 0;
}


// functia pentru preluarea mesajelor pentru istoric //
int preluareMesaje(void *data, int argc, char **argv, char **azColName){
    printf("Callback mesaje: Message: %s, Timestamp: %s, Sender: %s, Receiver: %s\n", argv[0], argv[1], argv[2], argv[3]);
    char *bufferIstoric = (char *)data;
    char linieMesaj[512]; // Mărită dimensiunea pentru a include numele utilizatorilor

    if(argc >= 4 && (strlen(bufferIstoric) + strlen(argv[0]) + strlen(argv[1]) + strlen(argv[2]) + strlen(argv[3]) + 10) < RESPONSE_SIZE){
        snprintf(linieMesaj, sizeof(linieMesaj), "%s -> %s: %s la %s\n", argv[2], argv[3], argv[0], argv[1]);
        strcat(bufferIstoric, linieMesaj);
    }

    return 0;
}


// functia pentru afisarea istoricului conversatiilor //
void history(thDate *td){

    //ConversatiiInfo info;
    info.numarConversatii = 0;
    char *err_msg = 0;

    char sqlConversatii[1024];
    snprintf(sqlConversatii, sizeof(sqlConversatii), "SELECT IdUtilizator1, IdUtilizator2 FROM Conversatii WHERE IdUtilizator1 = %d OR IdUtilizator2 = %d;", td->IdUser, td->IdUser);



    if(sqlite3_exec(db, sqlConversatii, preluareConversatii, info.conversatii, &err_msg) != SQLITE_OK){
        fprintf(stderr, "Eroare la preluarea conversațiilor: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    char bufferIstoric[RESPONSE_SIZE] = "";

    for (int i = 0; i < info.numarConversatii; i++) {
        char sqlMesaje[1024];
        snprintf(sqlMesaje, sizeof(sqlMesaje), "SELECT M.TextMesaj, M.Timestamp, UE.Username AS Expeditor, UD.Username AS Destinatar FROM Mesaje M JOIN Utilizatori UE ON M.IdExpeditor = UE.IdUser JOIN Utilizatori UD ON M.IdDestinatar = UD.IdUser WHERE (M.IdExpeditor = %d AND M.IdDestinatar = %d) OR (M.IdExpeditor = %d AND M.IdDestinatar = %d) ORDER BY M.Timestamp;",info.conversatii[i].IdUtilizator1, info.conversatii[i].IdUtilizator2, info.conversatii[i].IdUtilizator2, info.conversatii[i].IdUtilizator1);

        if(sqlite3_exec(db, sqlMesaje, preluareMesaje, bufferIstoric, &err_msg) != SQLITE_OK){
            fprintf(stderr, "Eroare la preluarea mesajelor: %s\n", err_msg);
            sqlite3_free(err_msg);
        }
    }

    if(strlen(bufferIstoric) == 0){
        snprintf(td->raspuns, RESPONSE_SIZE, "[server] Nu exista mesaje in istoricul dumneavoastra.\n");
    } else {
        snprintf(td->raspuns, RESPONSE_SIZE, "[server] Istoricul mesajelor dumneavoastra:\n%s", bufferIstoric);
    }

    trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

// functia pentru logout //
void logout(thDate *td){
  if(td->logat){
    char sql[256];
    sprintf(sql, "UPDATE Utilizatori SET online = 0 WHERE IdUser = %d;", td->IdUser);
    char *err_msg = 0;
    int bd = sqlite3_exec(db, sql, NULL, 0, &err_msg);
    if(bd!=SQLITE_OK){
      fprintf(stderr, "Eroare la actualizarea starii offline: %s\n.", err_msg);
      sqlite3_free(err_msg);
    }

    int i, j;
    for( i = 0; i < nrConexiuni; i++){
      if(conexiuniActive[i].IdUser == td->IdUser){
        for(j = i; j< nrConexiuni-1; j++){
          conexiuniActive[j]=conexiuniActive[j+1];
        }
        nrConexiuni--;
        break;
      }
    }
    td->logat = 0;
      snprintf(td->raspuns, RESPONSE_SIZE, "[server] Deconectare reusita!\n");
  }else{
      snprintf(td->raspuns, RESPONSE_SIZE, "[server} Nu Sunteti conectati la server.\n");
  }
  trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

// functia pentru a atentiona selectarea unei comenzi invalide //
void comandaInvalida(thDate *td){
  snprintf(td->raspuns, RESPONSE_SIZE, "[server] Comanda invalida. Selectati alta comanda.\n");
  trimiteRaspuns(td->cl, td->raspuns, td->IdThread);
}

void handleComanda(thDate *td, int optiune){
  char raspuns[256];
  printf("In handleComanda, optiunea primita: %d\n", optiune);

  if(optiune == 1){
    inregistrare(td);
  }
  else if(optiune == 2){
    login(td);
  }
  else if(optiune == 3){
    afisareUseriOnline(td);
  }
  else if(optiune == 4){
    newMessages(td);
  }
  else if(optiune == 5){
    reply(td);
  }
  else if(optiune == 6){
    sendNewMessages(td);
  }
  else if( optiune == 7){
    printf("ID Thread: %d, Client Socket: %d, User ID: %d\n", td->IdThread, td->cl, td->IdUser);
    history(td);
  }
  else if (optiune == 8){
    logout(td);
    td->deconectareThread = 1;
  }
  else{
    comandaInvalida(td);
  }
}

int mesajClient(void *arg, sqlite3 *db){
  struct thDate *tdL = (struct thDate*)arg;
  tdL->logat=0;  //initial utilizatorul nu este logat
  int optiune;

  while(1){

    if(tdL->deconectareThread){
      printf("[Thread %d] Clientul s-a deconectat.\n", tdL->IdThread);
      close(tdL->cl);
      return NULL;
    }
    if(read(tdL->cl, &optiune, sizeof(int)) == 0){
      printf("[Thread %d] Clientul s-a deconectat\n", tdL->IdThread);
      perror ("Eroare la read() de la client.\n");
      break;
    }

    printf("[Thread %d] Optiunea primita: %d\n", tdL->IdThread, optiune);
    

    if(tdL->logat==0){
      if(optiune == 1 || optiune == 2 ){
        handleComanda(tdL, optiune);
      }
      else{
        comandaInvalida(tdL);
      }
    }else if(tdL->logat==1){
      if(optiune >=3 && optiune <=8){
        handleComanda(tdL, optiune);
      }else{
      comandaInvalida(tdL);
      }
    }
  }
  
  close(tdL->cl);
  printf("[Thread %d] Conexiune inchisa.\n", tdL->IdThread);

}

int main ()
{
  struct sockaddr_in server;	
  struct sockaddr_in from;	
  int descr_sk, client;		
  int pid;
  pthread_t th[100];   
	int i=0;
  int lungime = sizeof(from);

  int bd = sqlite3_open("messenger.db" ,&db);
  if(bd != SQLITE_OK){
    fprintf(stderr, "Eroare la deschiderea bazei de date:F %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  /* crearea unui socket */
  descr_sk= socket(AF_INET, SOCK_STREAM, 0);
  if (descr_sk== -1){
      perror ("[server]Eroare la socket().\n");
      return errno;
  }
  
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(descr_sk, SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  

  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (descr_sk, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (descr_sk, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }
 

  while (1){
      int client;
      thDate * td;     
      int lungime = sizeof(from);
      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);

      /* acceptam un client (stare blocanta pana la realizarea conexiunii) */
      if ( (client = accept (descr_sk, (struct sockaddr *) &from, &lungime)) < 0){
	      perror ("[server]Eroare la accept().\n");
	      continue;
	    }
      
	td=(struct thDate*)malloc(sizeof(struct thDate));	
  td->deconectareThread = 0;
	td->IdThread=i++;
	td->cl=client;

	pthread_create(&th[i], NULL, &treat, td);  
	}
  sqlite3_close(db);
  return 0;
}

static void *treat(void * arg){ //functia executata de fiecare thread ce realizeaza comunicarea cu clientii
  struct thDate tdL;
  tdL= *((struct thDate*)arg);
  printf("[thread] - %d- Asteptam mesajul...\n", tdL.IdThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  mesajClient((struct thData*)arg, db);

  /*am terminat cu acest client, inchidem conexiunea*/
  close(tdL.cl);
  free(arg);
  return (NULL);

}