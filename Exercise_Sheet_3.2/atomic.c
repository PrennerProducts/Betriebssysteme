//
// Created by McRebel on 19.09.2022.
//

#include <pthread.h>
#include <stdio.h>


void* thread_func(void* arg) {
    int* mydata = (int*)arg;
    // Wir addieren zur Ã¼bergebenen Zahl 10000-mal die 1
    for (int i = 0; i < 10000; i++) {
        *mydata += 1;
    }
    return NULL;
}

int main() {
    // Reserviere Speicher fÃ¼r die Thread-Handles
    pthread_t myThreads[10];
    // Reserviere Speicher fÃ¼r die benutzerdefinierten Daten
    // Zu dieser Zahl werden 10 Threads 10000-mal die 1 dazuaddieren
    int mydata = 0;

    // Erzeuge 10 Threads
    for (int i = 0; i < 10; i++) {
        printf("Erzeuge Thread %d\n", i);
        // Erzeuge den Thread
        pthread_create(&myThreads[i], NULL, &thread_func, &mydata);
    }

    // Warte auf die Beendigung aller Threads
    for (int i = 0; i < 10; i++) {
        pthread_join(myThreads[i], NULL);
        printf("Thread %d wurde beendet\n", i);
    }

    // Gebe das Endergebnis aus
    // Wenn 10 Threads 10000-mal die 1 dazuaddieren, mÃ¼sste doch 100000 rauskommen
    // Tut es das?
    // Wenn es das tut, ist das immer der Fall?
    printf("mydata = %d\n", mydata);

    return 0;
}