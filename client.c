#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdint.h>

/* portul de conectare la server*/
int port;
int logat = 0;  //cu variabila asta urmarim starea de autentificare a fiecarui client

void Meniu(){
  printf("\n===== MENIU =====\n");
  if(logat == 0){
    printf("1. Inregistrare\n");
    printf("2. Logare\n");
  }
  else if(logat ==1){
    printf("3. Vezi utilizatorii activi\n");
    printf("4. Vezi mesaje noi\n");
    printf("5. Raspunde\n");
    printf("6. Trimite mesaj nou\n");
    printf("7. Vezi istoric\n");
    printf("8. Delogare\n");
  }
  printf("=================\n");
  printf("Alege o optiune: ");
}


int primireRaspuns(int descr_sk, char **raspuns){
  int len;

  if (read(descr_sk, &len, sizeof(len)) <0 ){
    printf("[client] Eroare le citirea lungimii.\n");
  }
  //printf("Lungimea mesajului primit: %d\n", len);

  *raspuns=(char*)malloc(len*sizeof(char));
  if(*raspuns == NULL){
      perror("[client] Eroare la alocarea memoriei\n");
      return errno;
  }

  memset(*raspuns, 0, len*sizeof(char));
  if(read(descr_sk, *raspuns, len) < 0){
    perror("[client] Eroare la read de la server.\n");
    free(*raspuns);
    return errno;
  }

  return 0;
}

int inregistrare(int descr_sk){
  char username[50], password[50], password2[50];
    printf("Introduceti username: ");
    scanf("%49s", username);

    do{
        printf("Introduceti parola: ");
        scanf("%49s", password);
        printf("Introduceti parola din nou: ");
        scanf("%49s", password2);

        if(strcmp(password, password2)!=0){
          printf("Parola pe care ati introdusa nu e la fel.\n. Incercati din nou.\n");
        }
    }while(strcmp(password, password2)!=0);


    //Trimitem optiunea la server
    int option = 1;
    write(descr_sk, &option, sizeof(int));

    //Trimitem usernameul si parola serverului
    write(descr_sk, username, 50);
    write(descr_sk, password, 50);

    //Citim si afisam raspunsul serverului
    char *raspunsServer = NULL;
    if(primireRaspuns(descr_sk, &raspunsServer)!=0){
      printf("Eroare la primirea raspunsul din functie.\n");
    }
    int result = 0;
    printf("Raspunsul de la server este: \n%s\n", raspunsServer);
    if(strstr(raspunsServer, "[server] Inregistrare efectuata cu succes! Incercati sa va logati!")!=NULL || strstr(raspunsServer, "[server] Username-ul deja exista. Incercati sa va logati.")!=NULL ){
      result  = 1;
    }
    free(raspunsServer);
    return result;
}

int login(int descr_sk){
  int optiune = 2;
  char username[50], password[50];

  printf("Introduceti username: ");
  scanf("%49s", username);
  printf("Introduceti parola: ");
  scanf("%49s", password);

  write(descr_sk, &optiune, sizeof(int));

  //Trimitem la server username-ul si parola
  write(descr_sk, username, 50);
  write(descr_sk, password, 50);
  
  //Citim si afisam raspunsul serverului
  char *raspunsServer = NULL;
  if(primireRaspuns(descr_sk, &raspunsServer)!=0){
    printf("Eroare la primirea raspunsul din functie.\n");
  }
  printf("Raspunsul de la server este: \n%s\n", raspunsServer);
  
  int result = 0;
  if(strstr(raspunsServer, "[server] V-ati logat cu succes! ID utilizator:")!=NULL){
    result = 1;
  }
  free(raspunsServer);
  return result;
}

int deconectare(int descr_sk){
  int optiune = 8;
  write(descr_sk, &optiune, sizeof(int));

  char *raspunsServer = NULL;
  if(primireRaspuns(descr_sk, &raspunsServer) != 0){
    printf("Eroare la primirea raspunsului de la server.\n");
    return 0;
  }

  printf("Raspunsul de la server este: \n%s\n", raspunsServer);
  int result = 0;
  if(strstr(raspunsServer, "[server] Deconectare reusita!")!=NULL){
    result = 1;
  }
  free(raspunsServer);
  return result;
}

void afisareUseriOnline(int descr_sk){
  int optiune = 3;
  char *raspuns = NULL;
  write(descr_sk, &optiune, sizeof(int));

  if(primireRaspuns(descr_sk, &raspuns) == 0){
    //printf("\n%s\n", raspuns);
    free(raspuns);
  }else{
    printf("Eroare la citirea utilizatorilor online.\n");
    return;
  }

}

void reply(int descr_sk){
  int optiune = 5;
  char numeDestinatar[50];
  int idMesajRaspuns;
  char mesaj[256];

  printf("Introduceti numele destinatarului: ");
  scanf("%49s", numeDestinatar);

  write(descr_sk, &optiune, sizeof(int));
  write(descr_sk, numeDestinatar, sizeof(numeDestinatar));

  char *conversatie = NULL;
  primireRaspuns(descr_sk, &conversatie);
  printf("Conversatia cu %s:\n%s\n", numeDestinatar, conversatie);

  if(strstr(conversatie, "[server] Nu exista mesaje in conversatia cu") == NULL){
    printf("Introduceri ID-ul mesajului la care doriti sa raspundeti: ");
    scanf("%d", &idMesajRaspuns);

  getchar();
  printf("Scrieti mesajul de raspuns: ");
  fgets(mesaj, sizeof(mesaj), stdin);

  write(descr_sk, &idMesajRaspuns, sizeof(int));
  write(descr_sk, mesaj, strlen(mesaj));

  char *raspunsServer = NULL;
  primireRaspuns(descr_sk, &raspunsServer);
  printf("Raspunsul de la server: \n%s", raspunsServer);
  free(raspunsServer);
  }
  free(conversatie);
}

void sendNewMessages(int descr_sk){
  int optiune = 6;
  char numeDestinatar[50];
  char mesaj[256];

  printf("Introduceti numele destinatarului: ");  //modificare cu cui ii trimiteti mesaj
  scanf("%49s", numeDestinatar);
  getchar();
  printf("Scrieti mesajul: ");
  fgets(mesaj, sizeof(mesaj), stdin);
  
  
  printf("Mesajul este: %s\n", mesaj);
  write(descr_sk, &optiune, sizeof(int));
  write(descr_sk, numeDestinatar, sizeof(numeDestinatar));
  write(descr_sk, mesaj, sizeof(mesaj));

  char *raspunsServer = NULL;
  primireRaspuns(descr_sk, &raspunsServer);
  printf("Raspunsul de la server: \n%s\n", raspunsServer);
  free(raspunsServer);
}


void newMessages(int descr_sk){
  int optiune = 4;
  write(descr_sk, &optiune, sizeof(int));

  char *raspunsServer = NULL;
  primireRaspuns(descr_sk, &raspunsServer);
  printf("Mesaje noi: \n%s\n", raspunsServer);
  free(raspunsServer);

}

void history(int descr_sk){
  int optiune = 7;
  write(descr_sk, &optiune, sizeof(int));

  char *raspunsServer = NULL;

  if(primireRaspuns(descr_sk, &raspunsServer) !=0){
    printf("%s\n", raspunsServer);
  }else{
    printf("Istoricul conversatiilor: \n%s\n", raspunsServer);
  }
  free(raspunsServer);
}

int comandaInvalida(int descr_sk, int optiune){
  write(descr_sk, &optiune, sizeof(int));

  char *raspunsServer = NULL;
  
  if( primireRaspuns(descr_sk, &raspunsServer) != 0){
    printf("Eroare la primirea raspunsului de la server.\n");
    return 0;
  }

  printf("Raspunsul de la server este: \n%s\n", raspunsServer);
  int result = 0;
  if(strstr(raspunsServer, "[server] Comanda invalida. Selectati alta comanda.") != NULL){
    result = 1;
  }
  
  free(raspunsServer);

  return result;
}

int main (int argc, char *argv[])
{
  int continua = 1;
  int optiune = 0;
  int descr_sk;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s 127.0.0.1 <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  descr_sk= socket(AF_INET, SOCK_STREAM, 0);

  if (descr_sk== -1)
    {
      perror ("Eroare la socket().\n");
      errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  server.sin_family = AF_INET;/* familia socket-ului */
  server.sin_addr.s_addr = inet_addr(argv[1]);  /* adresa IP a serverului */
  server.sin_port = htons (port);  /* portul de conectare */
  
  /* conectarea la server */
  if (connect (descr_sk, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }


  do{
    Meniu();
    scanf("%d", &optiune);
    //printf("Optiune selectata: %d\n", optiune);

    if(logat == 0){
    if(optiune == 1){
      if(inregistrare(descr_sk) ){
        if(login(descr_sk)){
          logat=1;
        }
      }
    } else if(optiune == 2){
        if(login(descr_sk)){
          logat=1;
        }
      }else{
        comandaInvalida(descr_sk, optiune);
      }
  }else{
      if(optiune == 3){
        afisareUseriOnline(descr_sk);
      }
      if(optiune == 4){
        newMessages(descr_sk);
      }else
      if(optiune == 5){
        reply(descr_sk);
      }else
      if(optiune == 6){
        sendNewMessages(descr_sk);
      }else
      if(optiune == 7){
        history(descr_sk);
      }else
        if(optiune == 8){
          if(deconectare(descr_sk)){
            logat = 0;
            continua = 0;
          }
        }else{
          comandaInvalida(descr_sk, optiune);
          continue;
        }
  }
  
  fflush(stdout);

  }while(continua);

  printf("Programul s-a incheiat.\n");

  /* inchidem conexiunea, am terminat */
  close (descr_sk);
  return 0;
}