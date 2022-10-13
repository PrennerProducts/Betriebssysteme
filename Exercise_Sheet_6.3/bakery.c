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

//Globale Variablen

#define LAGER_SIZE  120
#define LIEFERMENGE 500


// Globale Variablen
int call_Lieferant = 0;
int shutdownFlag = 0;
int lieferatn_informiert = 0;
int liefercount = 0;

typedef struct {
    int lagerstand;
    pthread_mutex_t mutex;
}lager_t;

lager_t lager;

pthread_cond_t lieferant_cond;
pthread_mutex_t lieferant_mutex;

pthread_cond_t backstation_cond;
pthread_mutex_t backstation_mutex;


void* bakingBread(void* arg){
        while(!shutdownFlag){
            pthread_mutex_lock(&lager.mutex);
            if(lager.lagerstand < LAGER_SIZE){
                lager.lagerstand += 1;
                if(lager.lagerstand >=((LAGER_SIZE/3)*2)) {
                    if(!lieferatn_informiert){
                    printf("Lieferant wird angerufen!\n");
                    pthread_mutex_lock(&lieferant_mutex);
                    pthread_cond_signal(&lieferant_cond);
                        printf("Lieferant Signal\n");
                    pthread_mutex_unlock(&lieferant_mutex);
                        lieferatn_informiert = 1;
                    sleep(1);
                    }
                }
                pthread_mutex_unlock(&lager.mutex);
            }else{
                pthread_mutex_lock(&backstation_mutex);
                // schlafen legen
                printf("Backstation geht schlafen!\n");
                pthread_mutex_unlock(&lager.mutex);

                pthread_cond_wait(&backstation_cond, &backstation_mutex);
                pthread_mutex_unlock(&backstation_mutex);
            }
        }
    pthread_mutex_unlock(&lager.mutex);
    return NULL;
}

void* deliverBread(void* arg){
    while (!shutdownFlag){
        pthread_mutex_lock(&lieferant_mutex);
        printf("Lieferant wartet noch\n");
        pthread_cond_wait(&lieferant_cond, &lieferant_mutex);
        printf("Lieferant wurde aufgeweckt`!\n");
        pthread_mutex_unlock(&lieferant_mutex);
        sleep(4);
        pthread_mutex_lock(&lager.mutex);
        liefercount += lager.lagerstand;
        lager.lagerstand  = 0;
        printf("Lager wurde geleert!\n");
        lieferatn_informiert = 0;
        // Wecke Backstationen auf
        pthread_mutex_lock(&backstation_mutex);
        pthread_cond_broadcast(&backstation_cond);
        pthread_mutex_unlock(&backstation_mutex);
        pthread_mutex_unlock(&lager.mutex);
        printf("Backstationen wurden mit broadcast aufgeweckt!\n");
        if(liefercount >= LIEFERMENGE){
            printf("Liefermenge ist erreicht: Programm wird beendet!\n");
            shutdownFlag = 1;
        }
    }
    return NULL;
}

void qualityCheck(int signum){
    printf("QualityCheck()\n");
    int myrandnum = 1 + rand() % 5;
    pthread_mutex_lock(&lager.mutex);
    lager.lagerstand -= myrandnum;
    pthread_mutex_unlock(&lager.mutex);

    // Wecke Backstationen auf
    pthread_mutex_lock(&backstation_mutex);
    pthread_cond_broadcast(&backstation_cond);
    pthread_mutex_unlock(&backstation_mutex);
    pthread_mutex_unlock(&lager.mutex);
    alarm(2);
}


// Das ist der Signal-Handler, der fÃ¼r den graceful shutdown zustÃ¤ndig ist
void graceful_shutdown_handler(int signum) {
    pthread_cond_broadcast(&backstation_cond);
        shutdownFlag = 1;
    pthread_mutex_lock(&backstation_mutex);
    pthread_cond_broadcast(&backstation_cond);
    pthread_mutex_unlock(&backstation_mutex);

    pthread_mutex_lock(&lieferant_mutex);
    pthread_cond_broadcast(&lieferant_cond);
    pthread_mutex_unlock(&lieferant_mutex);

        printf("Ok, Ich beende mich.\n");
        exit(0);
}


int main() {
    lager.lagerstand = 0;


    // INIT Mutex und ConditionVariable
    pthread_mutex_init(&lager.mutex,NULL);
    pthread_mutex_init(&lieferant_mutex, NULL);
    pthread_mutex_init(&backstation_mutex, NULL);
    pthread_cond_init(&lieferant_cond, NULL);
    pthread_cond_init(&backstation_cond, NULL);

    // Signalhändler für AlarmSignal
    struct sigaction my_signal;
    my_signal.sa_handler = qualityCheck;
    sigemptyset(&my_signal.sa_mask);
    my_signal.sa_flags = 0;
    sigaction(SIGALRM, &my_signal, NULL);
    alarm(2);


    struct sigaction gracefull_Shutdown;
    my_signal.sa_handler = qualityCheck;
    sigemptyset (&gracefull_Shutdown.sa_mask);
    sigaddset(&gracefull_Shutdown.sa_mask, SIGTERM);
    sigaddset(&gracefull_Shutdown.sa_mask, SIGINT);
    sigaddset(&gracefull_Shutdown.sa_mask, SIGQUIT);
    gracefull_Shutdown.sa_flags = 0;
    sigaction(SIGINT, &gracefull_Shutdown, NULL);



    // Gracefull Shutdown:

    if (sigaction(SIGINT, &gracefull_Shutdown, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }





    pthread_t lieferantThread;
    pthread_create(&lieferantThread, NULL, deliverBread, NULL);

    pthread_t backThreads[6];
    for (int i = 0; i < 6; ++i) {
        pthread_create(&backThreads[i], NULL, bakingBread, NULL);
    }



    // Warte auf die Threads
    for (int i = 0; i < 6; ++i) {
        pthread_join(backThreads[i], NULL);
    }
    pthread_join(lieferantThread, NULL);

    //Destroy Mutex & Cond
    pthread_mutex_destroy(&lager.mutex);
    pthread_mutex_destroy(&lieferant_mutex);
    pthread_mutex_destroy(&backstation_mutex);
    pthread_cond_destroy(&backstation_cond);
    pthread_cond_destroy(&lieferant_cond);

    return 0;



}
