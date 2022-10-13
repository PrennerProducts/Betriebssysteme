/*Damit ein Programm, das Shared-Memory verwendet, korrekt erzeugt werden
kann, muss beim Linken das Argument -lrt übergeben werden!
• Shared-Memory ist in der Programmbibliothek librt implementiert.
• Das angegebene Argument verknüpft diese Bibliothek mit dem Programm.
• z.B.: gcc -o myprogram myprogram.c -lrt*/

#include <signal.h>
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



#define SharedMemName "/bankingsystem"
#define AnzahlAccounts  12
#define Credit_Amount 400
#define Child_Prozess_Count  4

// Globale Variablen:

int shutdownFlag = 0;
int count = 0;

typedef struct{
    int accountNr;
    float balance;
    float creditsum;
}bankaccount_t;

typedef struct{
    bankaccount_t bancaccounts[AnzahlAccounts];
    pthread_mutex_t mutex;
}bankingsystem_t;

bankingsystem_t* data;

void clientAccessAccount(float ammount,int accountNr){
    pthread_mutex_lock(&data->mutex);
    data->bancaccounts[accountNr].balance += ammount;
    pthread_mutex_unlock(&data->mutex);
}

void serverReadAccounts(){
    pthread_mutex_lock(&data->mutex);

        printf("\n#############################################\n");
        printf("   All     Bankaccounts PrintCount: %d \n", count++);
        printf("#############################################\n");
    for (int i = 0; i < AnzahlAccounts ; ++i) {
        if(data->bancaccounts[i].balance < 0){
            data->bancaccounts[i].balance += Credit_Amount;
            data->bancaccounts[i].creditsum += Credit_Amount;
        }
        printf("Balance of Account %d: %f\n",i, data->bancaccounts[i].balance);
    }
    pthread_mutex_unlock(&data->mutex);
}
//SignalHandler
void mySignalHandler(int signum){
    if(signum == SIGALRM){
        serverReadAccounts();
        alarm(3);
    } else{
        shutdownFlag = 1;
    }
}

int main() {
    // Initialize random numbers
    srand(time(NULL));

    //SHM Schritt 1 Create SHaredMEmmory Segment
    int myShMemSegment = shm_open(SharedMemName, O_CREAT | O_EXCL | O_RDWR, 0666);
    if(myShMemSegment == -1){
       if(errno == EEXIST){
           perror("Es existiert bereits ein SharedMemmorySegment mit diesem Namen!");
           exit(-31);
       } else{
           perror("ERROR bei create Segment!");
      }
    }
    printf("Create: SHM Segment\n");

    // SHM Schritt 2: ShMemmory Größe einstellen
    ftruncate(myShMemSegment, sizeof(bankingsystem_t));

    //SHM Schritt 3: ShMemory in den Speicherbereich einhängen
    void* shMemPtr = mmap(0, sizeof (bankingsystem_t),PROT_WRITE, MAP_SHARED, myShMemSegment, 0);
    printf("Mount: SHM Segment\n");

    // SHM SChritt 4: Cast das SHM Segment zum richtigen Datentyüen
    data = (bankingsystem_t*) shMemPtr;


    // Mutex Init
    pthread_mutex_init(&data->mutex, NULL);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (pthread_mutex_init(&data->mutex, &attr) != 0) {
        fprintf(stderr, "Could not initialize the mutex.\n");
        // Free all ressources
        munmap(shMemPtr, sizeof(bankingsystem_t));
        close(myShMemSegment);
        shm_unlink(SharedMemName);
        exit(1);
    }

    // Initialize the banking accounts
    for(int i = 0; i < AnzahlAccounts; i++) {
        float val = (float)((rand()%100000)/100.0);
        data->bancaccounts[i].balance = val;
        data->bancaccounts[i].creditsum = 0;
    }

    // Initialize the signal handler
    // The signal handler will be inherited by all child processes
    // The children will never call alarm() therefore we can savely use the same signal handler for the parent and for the children
    struct sigaction my_signal;
    my_signal.sa_handler = mySignalHandler;
    sigemptyset (&my_signal.sa_mask);
    my_signal.sa_flags = 0;
    if (sigaction(SIGALRM, &my_signal, NULL) != 0
        || sigaction(SIGINT, &my_signal, NULL) != 0
        || sigaction(SIGTERM, &my_signal, NULL) != 0
        || sigaction(SIGQUIT, &my_signal, NULL) != 0) {
        perror("Could not register signal handler");
        // Free all ressources
        pthread_mutex_destroy(&data->mutex);
        munmap(shMemPtr, sizeof(bankingsystem_t));
        close(myShMemSegment);
        shm_unlink(SharedMemName);
        // Exit Kindprozess sonst laufen sie weiter!
        exit(1);
    }

    // Fork() Create the child processes which all call clientAccessAccount() in an endless loop
    pid_t pids[Child_Prozess_Count];
    for(int i = 0; i < Child_Prozess_Count; i++) {
        // Create the child process
        pids[i] = fork();
        // An error happened
        if (pids[i] < -1) {
            perror("Could not create child process");

        } else if (pids[i] == 0) {// Code of the child process
            // Reinitialize random numbers (so that we don't get the same numbers in all child processes)
            srand(time(NULL));
            while(!shutdownFlag) {
                //printf("Child:\n");  // DEBUG Printf
                // Call clientAccessAccount() with random amount and random account
                float amount = (float)(rand()%10000)/100.0 - 49.995;
                int accountNr = rand()%AnzahlAccounts;
                clientAccessAccount(amount, accountNr);
                // Let's sleep a bit
                sleep(1);
            }
            // Exit the child process
            printf("Shutting down child process...\n");
            exit(0);
        }
    }

    // Set the alarm
    // Afterwards we need to sleep otherwise we would constantly reset the alarm (so that the corresponding signal is never delivered)
    // pause will wait for any signal
     while(!shutdownFlag) {
        alarm(3);
        pause();
    }



    // Wait for the children to finish
    for(int i = 0; i < Child_Prozess_Count; i++) {
        waitpid(pids[i], NULL, 0);
    }


    // Free all ressources
    printf("Shutting down parent process...\n");
    pthread_mutex_destroy(&data->mutex);
    munmap(shMemPtr, sizeof(bankingsystem_t));
    close(myShMemSegment);
    shm_unlink(SharedMemName);
}



