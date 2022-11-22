#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

//colori per la console
#define GRN "\x1B[32m"
#define YLW "\x1B[33m"
#define DFLT "\033[0m"


char* printTime(void){
    char s[256];
    time_t timet;
    time(&timet);
    struct tm *pTm = localtime(&timet);
    sprintf(s, "[%02d:%02d:%02d] ", pTm->tm_hour, pTm->tm_min, pTm->tm_sec);
    return strdup(s);
}

typedef struct node
{
    int numero;
    int PID;
    struct node *next;
} node;

struct datiAereo{
    int PID;
    int numero;
};

long get_random(long lMin, long lMax) {
    int bSeed = 0;
    if(!bSeed) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srandom(tv.tv_usec % 1000);
        bSeed = 1;
    }
    long l = random();
    return ((l % (lMax - lMin + 1L)) + lMin);
}

int aereo(int i){
    int pista;
    int torrePID = -1;

    void handler(int signo, siginfo_t *info, void *extra){ //handler per il segnale SIGUSR1 
        pista = info->si_value.sival_int;
        torrePID = info->si_pid;
        return;
    }
    
    struct sigaction sa;    
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO; //questa flag mi permette di ricevere i dati allegati al signal
    sigaction(SIGUSR1, &sa, NULL); 

    //printf("%s[Aereo %d] Creato.\n", printTime(), i+1); 

    int attesa = get_random(3,8);
    printf("%s[Aereo %d] Inizio a prepararmi per %d secondi.\n", printTime(), i+1, attesa);
    sleep(attesa);

    int req = open("richieste", O_WRONLY); //apro il file richieste in modalità di scrittura
    if(req == -1){
        printf("%s[Aereo %d] Errore nell'apertura del pipe.\n", printTime(), i+1);
        printf("Errore: %d\n", errno);
        return -1;
    }

    struct datiAereo dati; //struct che contiene i dati dell'aereo
    dati.PID = getpid();
    dati.numero = i;

    write(req, &dati, sizeof(dati)); 
    //scrivo nella pipe i dati dell'aereo, in modo che la torre sappia il PID e il numero dell'aereo
    //avrei potuto evitare di usare la struct e avere il campo PID perchè esiste un altro modo di ottenerlo tramite l'handler, 
    //ma in questo punto del progetto non lo sapevo ancora

    printf("%s[Aereo %d] Inviata richiesta di decollo alla torre.\n", printTime(), i+1);
    pause(); // vado in attesa passiva finchè non ricevo il segnale SIGUSR1 dalla torre
    int decollo = get_random(5,15);
    
    printf("%s[Aereo %d] Inizio decollo in pista %d. Ci metterò %f secondi.\n", printTime(), i+1, pista+1, decollo);
    sleep(decollo);

    printf("%s[Aereo %d] Decollo completato.\n", printTime(), i+1);
    sigqueue(torrePID, SIGUSR1, (union sigval) i);      //mando segnale alla torre che ho finito il decollo, allegando il numero dell'aereo
    return 0;
}

int torre(int piste[], int nPiste, int nAerei){

    void handler(int signo, siginfo_t *info, void *extra){ //handler per il segnale SIGUSR1 
        //printf("%s[Torre] Ricevuto segnale SIGUSR1 da %d.\n", printTime(), info->si_pid);
        for(int i = 0; i < nPiste; i++){
            if(piste[i] == info->si_value.sival_int){
                piste[i] = -1;
                printf("%s%s[Torre] Aereo %d decollato. Pista %d liberata.%s\n",YLW, printTime(), info->si_value.sival_int+1, i+1, DFLT);
                break;
            }
        }
        return;
    }

    struct sigaction sa;         
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);


    struct node *front = NULL, *rear = NULL; //lista di aerei in attesa di decollo, inizializzo front e rear a NULL

    void enqueue(int n, int PID) //Funzione per inserire un aereo in coda
    {
        struct node *newNode = malloc(sizeof(struct node));
        newNode->numero = n;
        newNode->PID = PID;
        newNode->next = NULL;

        //Se è il primo nodo:
        if(front == NULL && rear == NULL)
            //Faccio in modo che front e rear puntino al nuovo nodo
            front = rear = newNode;
        else
        {
            //aggiungo newNode come next dell'ultimo in coda
            rear->next = newNode;

            //faccio divenire ultimo il nuovo nodo
            rear = newNode;
        }
    }

    void dequeue() //Funzione per rimuovere l'aereo nel front dalla coda (logica FIFO)
    {
        struct node *temp;
        if(front == NULL)
            printf("La coda è vuota, non posso eseguire il dequeue()\n");
        else
        {
            //Faccio un backup del primo nodo
            temp = front;
            //Faccio in modo che front punti al prossimo nodo
            front = front->next;
            //Se front == NULL, allora setto rear = NULL
            if(front == NULL)
                rear = NULL;

        //Libero il primo nodo
        free(temp);
        }
    }

    int libera(){ //funzione che ritorna il numero di una pista se è libera , -1 altrimenti
        for(int i = 0; i < nPiste; i++){
            if(piste[i] == -1){
                return i;
            }
        }
        return -1;
    }

    bool tutteLibere(){ //funzione che ritorna true se tutte le piste sono libere, false altrimenti
        for(int i = 0; i < nPiste; i++){
            if(piste[i] != -1){
                return false;
            }
        }
        return true;
    }
    
    void occupaPista(int i, int n){ //funzione che occupa la pista i con l'aereo n
        piste[i] = n;
    }

    void mandaAereo(node front, int pista){ //funzione che manda l'aereo front.numero alla pista libera
        int n = front.numero;
        int PID = front.PID;
        dequeue();
        occupaPista(pista, n);
        sigqueue(PID, SIGUSR1, (union sigval) pista); //mando segnale con allegato il numero della pista
        printf("%s%s[Torre] Aereo %d mandato in pista %d.%s\n", YLW, printTime(), n+1, pista+1, DFLT);
    }

    printf("%s%s[Torre] Torre operativa. Ci sono %d aerei da gestire.%s\n", YLW, printTime(), nAerei, DFLT);
    
    //inizializzo il pipe per le richieste
    mkfifo("richieste", 0777);
    int req = open("richieste", O_RDWR); //non so perchè, ma se uso la flag O_RDONLY non funziona bene il programma
    fcntl(req, F_SETFL, O_NONBLOCK); //imposto il pipe come non bloccante, in modo da poter continuare l'esecuzione anche se non ci sono richieste
    bool flag = false; //flag per controllare se ci sono state richieste (mi servirà per non uscire dal while subito)
    while (true){
        sleep(1);
        struct datiAereo dati; //dati che ricevo dal pipe
        if (read(req, &dati, sizeof(dati)) == -1){// se non arrivano richieste...
            int pista = libera();                   //...controllo se ci sono piste libere
            if((pista != -1) && (front != NULL)){ // se c'è una pista libera e un aereo in coda...
                mandaAereo(*front, pista);          // ...allora mando l'aereo in pista              
            } else if (front == NULL && tutteLibere() && flag){ //se non ci sono più aerei in coda e tutte le piste sono libere e la flag è true...
                break;                                             //...esco dal while
            }
        }   else{
            printf("%s%s[Torre] Ricevuta richiesta di decollo dall'aereo %d.%s\n", YLW,printTime(), dati.numero+1, DFLT);
            enqueue(dati.numero, dati.PID);
            int pista = libera();
            if((pista != -1)){
                mandaAereo(*front, pista);
                flag = true;
            } else {
                printf("%s%s[Torre] Non ci sono piste libere.%s\n",YLW, printTime(), DFLT);
            }
        }
        //for (int i = 0; i < nPiste; i++)              //controllo piste (debug)
        //{
        //    printf("%s[Torre] Pista %d: %d\n", printTime(), i+1, piste[i]);
        ///}
    }
    printf("%s%s[Torre] Fine Controllo.%s\n", YLW ,printTime(), DFLT);
    return 0;
}

int hangar(int nAerei){
    int status = 0; //status della fork
    int aerei[nAerei]; 
    printf("%s%s[Hangar] Hangar operativo. Inizio creazione di %d aerei.%s\n",GRN, printTime(), nAerei, DFLT);
    for (int i = 0; i < nAerei; ++i) { //creo i processi aerei
        if ((aerei[i] = fork()) < 0) { //se la fork fallisce...
            perror("fork");
            abort();
        } else if (aerei[i] == 0) { //se la fork ritorna 0, allora sono un aereo
            aereo(i);
            exit(0);
        } else { //se la fork ritorna un numero maggiore di 0, allora sono Hangar
            printf("%s%s[Hangar] Aereo %d creato.%s\n",GRN, printTime(), i+1, DFLT);
        }
        sleep(1);
    }
    printf("%s%s[Hangar] Creati %d Aerei.%s\n", GRN,printTime(), nAerei, DFLT);
    while (wait(&status) > 0); //aspetto che tutti i processi aerei finiscano
    printf("%s%s[Hangar] Non ci sono più aerei da far decollare.%s\n",GRN, printTime(), DFLT);
    return 0;
}

int main(){

    unlink("richieste"); //rimuovo il pipe per le richieste

    int nAerei = get_random(10,30); 
    int nPiste = get_random(2, 15);
    int piste[nPiste]; 
    
    for(int i = 0; i < nPiste; i++){ //inizializzo tutte le piste a -1
        piste[i] = -1;
    }

    printf("%s[Aeroporto] Abbiamo %d piste disponibili.\n", printTime(), nPiste);
    printf("%s[Aeroporto] Apriamo l'hangar e la torre.\n", printTime());

    int res = fork();
    if (res <  0){printf("%s[Aeroporto] Fork hangar fallito.\n", printTime());} //ERRORE
    else if (res == 0){hangar(nAerei);}                                         //HANGAR
    else                                                                        //AEROPORTO
    {
        int status;
        int res = fork();
        if (res <  0){printf("%s[Aeroporto] Fork torre fallito.\n", printTime());}//ERRORE
        else if (res == 0){torre(piste, nPiste, nAerei);}                         //TORRE
        else                                                                      //AEROPORTO
        {
            while (wait(&status) > 0); //una volta che tutti i figli sono morti
            printf("%s[Aeroporto] Torre e hangar terminati.\n", printTime());
            return 0;
        }
    }
}