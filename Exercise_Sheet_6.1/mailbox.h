//
// Created by McRebel on 07.10.2022.
//

#ifndef EXERCISE_SHEET_6_1_MAILBOX_H
#define EXERCISE_SHEET_6_1_MAILBOX_H

#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> /*  sleep   */
#include <string.h>
#include <signal.h>
#include <fcntl.h>


#define SERVER_QUEUE_NAME	"/Printserver"
#define CLIENT_QUEUE_NAME	"/Printclient.%d"


// Repräsentiert die Struktur der Nachricht vom Client zum Server
typedef struct {
    char name[30]; // Name des Druckauftrags
    char message[30]; // Inhalt des Druckauftrags
    char mailbox[50]; // Name der Client-Mailbox für die Rückantwort
} Content_t;


// Repräsentiert die Struktur der Rückantwort vom Server zum Client
typedef struct {
    char message[100];
} ServerResponse_t;


/**
 * Öffnet bzw. erzeugt die Message Queue mit den angegebenen Namen.
 *
 * @param name Name der Mailbox
 * @param attr In die übergebene Struktur werden die Attribute der Mailbox gespeichert für die weitere Verwendung im aufrufenden Programm
 * @param serverMailBox Wenn 1 soll eine Servermailbox angelegt werden, ansonsten eine Clientmailbox (unterscheiden sich in der Nachrichtengröße)
 * @return message queue descriptor
 */
static mqd_t connectToMailbox(char* name, struct mq_attr* attr, int serverMailbox) {
    struct mq_attr attributes;
    attributes.mq_maxmsg = 10;
    attributes.mq_msgsize = serverMailbox==1 ? sizeof(Content_t) : sizeof(ServerResponse_t);
    attributes.mq_flags = 0;
    attributes.mq_curmsgs = 0;

    *attr = attributes;

    mqd_t mqd = mq_open(name, O_CREAT | O_RDWR, 0666, &attributes);
    if(mqd == -1) {
        mq_close(mqd);
        perror("Could not open message queue");
        exit(1);
    }
    return mqd;
}


// Registriert den übergebenen Funktionspointer als Signal-Handler für einen Graceful Shutdown
static void registerShutdownHandler(void* fnc) {
    struct sigaction my_signal;

    my_signal.sa_handler = fnc;
    sigemptyset (&my_signal.sa_mask);
    sigaddset(&my_signal.sa_mask, SIGTERM);
    sigaddset(&my_signal.sa_mask, SIGINT);
    sigaddset(&my_signal.sa_mask, SIGQUIT);
    my_signal.sa_flags = 0;

    if (sigaction(SIGTERM, &my_signal, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }
    if (sigaction(SIGINT, &my_signal, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }
    if (sigaction(SIGQUIT, &my_signal, NULL) != 0) {
        perror("Fehler beim Registrieren des Signal-Handlers");
        exit(1);
    }
}
#endif //EXERCISE_SHEET_6_1_MAILBOX_H
