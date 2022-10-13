//
// Created by McRebel on 07.10.2022.
//
#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> /*  sleep   */
#include "mailbox.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>




// Globale Variablen
char mailboxname[50];
mqd_t servermailbox = -1;
mqd_t mailbox = -1;


// Forward declaration
void generateRandomMessage(char buffer[30]);


// Shutdown flag for graceful shutdown
int shutdownFlag = 0;


void shutdownHandler() {
    // Gebe alle Ressourcen wieder frei
    if (mailbox != -1) {
        mq_close(mailbox);
        mq_unlink(mailboxname);
    }
    if (servermailbox != -1) {
        mq_close(servermailbox);
    }
    exit(0);
}


int main(int argc, char* argv[]) {
    // Initialisiere Zufallszahlen
    srand(time(NULL));

    // Registriere den Signal-Handler für Graceful Shutdown
    registerShutdownHandler(shutdownHandler);

    // Überprüfe Anzahl der Eingabeparameter
    if(argc < 3) {
        fprintf(stderr, "Not enough parameters. Program call needs to meet the following structural requirements: ./client <char* name> <int priority>\n");
        return 1;
    }

    // Eingabeparameter Priority
    int priority;
    char *prioEnd;
    priority = strtol(argv[2], &prioEnd, 10);
    if(*prioEnd!='\0' || priority <= 0){
        fprintf(stderr, "Parameter for 'priority' is not an integer.\n");
        return 1;
    }

    // Eingabeparameter Name
    char* name = argv[1];

    // Baue Namen der Clientmailbox zusammen (Prefix + PID)
    snprintf(mailboxname, 50, CLIENT_QUEUE_NAME, getpid());

    // Erzeuge die Clientmailbox
    struct mq_attr attr;
    mailbox = connectToMailbox(mailboxname, &attr, 0);

    // Öffne die Servermailbox
    struct mq_attr serverAttr;
    servermailbox = connectToMailbox(SERVER_QUEUE_NAME, &serverAttr, 1);

    // Erzeuge Nachricht mit dem Druckauftrag
    Content_t content;
    generateRandomMessage(content.message);
    strncpy(content.mailbox, mailboxname, 50);
    strncpy(content.name, name, 30);

    // Sende Druckauftrag an den Server
    if (mq_send(servermailbox, (const char*)&content, sizeof(Content_t), priority) == -1) {
        perror("Error receiving");
        exit(1);
    }

    // Warte auf die Rückantwort des Servers
    char buffer[attr.mq_msgsize];
    if(mq_receive(mailbox, buffer, attr.mq_msgsize, NULL) == -1) {
        perror("Error receiving");
        exit(1);
    }

    // Gebe Nachricht des Servers aus
    ServerResponse_t* receivedMsg = (ServerResponse_t*)buffer;
    printf("%s\n", receivedMsg->message);

    // Gebe alle Ressourcen wieder frei
    mq_close(mailbox);
    mq_close(servermailbox);
    mq_unlink(mailboxname);
}


void generateRandomMessage(char buffer[30]) {
    int messageLength = 1 + rand()%29;
    for(int i = 0; i < messageLength; i++) {
        char c = 65 + rand()%57;
        buffer[i] = c;
    }
    buffer[messageLength] = '\0';
}

