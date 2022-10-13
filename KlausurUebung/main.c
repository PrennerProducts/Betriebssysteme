#include <stdio.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>           /* For O_* constants */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>


#define MAXSTORAGESPACE  30
#define TWOTHIRDSPACE 20
#define BREADORDERAMMOUNT 100
// Struct represents my Storage!
typedef struct{
    int storageitems;
    int total_deliverd_bread;
    pthread_mutex_t storage_mutex;
}storage_t;

storage_t myStorage;

// Contition Variablen and Mutex for CondVar
pthread_cond_t baking_cond;
pthread_mutex_t baking_mutex;
pthread_cond_t deliver_cond;
pthread_mutex_t deliver_mutex;

//Globale Variablen
int shutdownflag = 0;
int lieferantisinformed = 0;

void* bakingBread(void* arg){
    while (!shutdownflag) {
        pthread_mutex_lock(&myStorage.storage_mutex);
        if (myStorage.storageitems < MAXSTORAGESPACE){
            myStorage.storageitems += 1;
             printf("Ein Brot wird gebacken!\n");
             if (myStorage.storageitems >= TWOTHIRDSPACE && !lieferantisinformed) {
                // Lieferant informieren
                 printf("Lager ist zu 2/3 voll, Lieferant wird informiert!\n");
                 pthread_mutex_lock(&deliver_mutex);
                 pthread_cond_signal(&deliver_cond);
                 pthread_mutex_unlock(&deliver_mutex);
                 lieferantisinformed = 1;
                }
            pthread_mutex_unlock(&myStorage.storage_mutex);
        } else{
            pthread_mutex_unlock(&myStorage.storage_mutex);
            // Schlafen legen das Lager ist Voll!
            printf("BakingThread geht schlafen, lager ist voll!\n");
            pthread_mutex_lock(&baking_mutex);
            pthread_cond_wait(&baking_cond, &baking_mutex);
            pthread_mutex_unlock(&baking_mutex);
        }
    }
    return NULL;
}


void* deliverBread(void* arg ){
    while (!shutdownflag){
        //Schlafe bis benachrichtigung
        pthread_mutex_lock(&deliver_mutex);
        pthread_cond_wait(&deliver_cond, &deliver_mutex);
        pthread_mutex_unlock(&deliver_mutex);
        //danach warte kurz
        pthread_mutex_lock(&myStorage.storage_mutex);
        int myrandtime = 1+rand() % 3;
        printf("LieferantenThrad wurde geweckt! Er kommt in %d Sekunden\n", myrandtime);
        sleep(myrandtime);
        myStorage.total_deliverd_bread += myStorage.storageitems;
        myStorage.storageitems = 0;
        lieferantisinformed = 0;
        printf("Lager wurde geleert!\n");


        // Wenn die Bestellte Menge Brot ausgeliefert wurde Beende das Programm
        if(myStorage.total_deliverd_bread >= BREADORDERAMMOUNT){
            printf("Die Bestellte Menge Brot wurde ausgeliefert!\n");
            shutdownflag = 1;
        }
        pthread_mutex_unlock(&myStorage.storage_mutex);
    }
    return NULL;
}

void qualityCheck(int signum){
    alarm(2);
    pthread_mutex_lock(&myStorage.storage_mutex);
    printf("qualityCheck()  !!!\n");
    int qualitynum = 1 + rand() % 8;
    myStorage.storageitems -= qualitynum;
    pthread_mutex_unlock(&myStorage.storage_mutex);
}

void graceFullShutdownHandel(int signum){
    shutdownflag = 1;
    printf("GraceFullShutdown alle Threads werden aufgeweckt und resourcen werden aufgeräumt!\n");
    pthread_mutex_lock(&baking_mutex);
    pthread_cond_broadcast(&baking_cond);
    pthread_mutex_unlock(&baking_mutex);
    pthread_mutex_lock(&deliver_mutex);
    pthread_cond_signal(&deliver_cond);
    pthread_mutex_unlock(&deliver_mutex);
}


int main() {
    srand(time(NULL));

    // INIT Struct Storage
    myStorage.storageitems = 0;
    myStorage.total_deliverd_bread = 0;

    // INIT MUTEX und CONT
    pthread_mutex_init(&myStorage.storage_mutex,NULL);
    pthread_mutex_init(&baking_mutex, NULL);
    pthread_mutex_init(&deliver_mutex, NULL);
    //CONT
    pthread_cond_init(&baking_cond,NULL);
    pthread_cond_init(&deliver_cond, NULL);

    // INIT SIGNAL-Handler für qualityCheck()
    struct sigaction myQualitycheckSignal;
    myQualitycheckSignal.sa_handler = qualityCheck;
    myQualitycheckSignal.sa_flags = 0;
    sigemptyset(&myQualitycheckSignal.sa_mask);

    if(sigaction(SIGALRM, &myQualitycheckSignal, NULL)!= 0){
        perror("ERROR registering Signal_Handler!\n");
    }
    alarm(2);

    // INIT SIGAL_Handler für GraceFullShutdown

    struct sigaction myGraceShutdown;
    myGraceShutdown.sa_handler = graceFullShutdownHandel;
    myGraceShutdown.sa_flags = 0;
    sigemptyset (&myGraceShutdown.sa_mask);
    sigaddset(&myGraceShutdown.sa_mask, SIGTERM);
    sigaddset(&myGraceShutdown.sa_mask, SIGINT);
    sigaddset(&myGraceShutdown.sa_mask, SIGQUIT);
    myGraceShutdown.sa_flags = SA_RESTART;
    // Registriere den Signal-Handler fÃ¼r die Signale SIGTERM, SIGINT und SIGQUIT
    if (sigaction(SIGTERM, &myGraceShutdown, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }
    if (sigaction(SIGINT, &myGraceShutdown, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }
    if (sigaction(SIGQUIT, &myGraceShutdown, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }




    // Erzeuge Threads + INIT Threads
    pthread_t deliverThread;
    pthread_t bakingThreads[8];

    pthread_create(&deliverThread, NULL, deliverBread, NULL);
    printf("Erzeuge DeliverThread\n");

    for (int i = 0; i < 8; ++i) {
        printf("Erzeuge BakingThread Nr: %d\n", i);
        pthread_create(&bakingThreads[i], NULL, bakingBread, NULL);
    }



    // Destroy Threads
    pthread_join(deliverThread, NULL);
    printf("Destroy DeliverThread!\n");
    for (int i = 0; i < 8; ++i) {
        printf("Destroy BakingThread NR: %d\n", i);
        pthread_join(bakingThreads[i], NULL);
    }

    // Destroy MUTEX und CONT
    pthread_mutex_destroy(&myStorage.storage_mutex);
    printf("Destroy Storage_Mutex\n");
    pthread_cond_destroy(&baking_cond);
    printf("Destroy Baking_Cond\n");
    pthread_cond_destroy(&deliver_cond);
    printf("Destroy Deliver_Cont\n");




    return 0;
}
