#include <mqueue.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mailbox.h"
#include <string.h>
#include <time.h>
#include <errno.h>
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

// Shutdown flag for graceful shutdown
int shutdownFlag = 0;


// Forward declaration
void bearbeiteDruckauftrag(Content_t* msg);


void shutdownHandler() {
    shutdownFlag = 1;
}


int main() {
    // Initialisiere Zufallszahlen
    srand(time(NULL));

    // Registriere den Signal-Handler für Graceful Shutdown
    registerShutdownHandler(shutdownHandler);

    // Erzeuge die Servermailbox
    struct mq_attr attr;
    mqd_t serverMailbox = connectToMailbox(SERVER_QUEUE_NAME, &attr, 1);

    int signalReceived = 0; // Speichert ob mq_receive() durch ein Signal unterbrochen wurde
    while(!shutdownFlag) {
        // Empfange Nachricht
        char buffer[attr.mq_msgsize];
        unsigned priority;
        if(mq_receive(serverMailbox, buffer, attr.mq_msgsize, &priority) == -1) {
            if (errno == EINTR) { // mq_receive() wurde durch ein Signal unterbrochen
                signalReceived = 1;
            } else { // Sonstiger Fehler
                // Gebe Ressourcen wieder frei
                mq_close(serverMailbox);
                mq_unlink(SERVER_QUEUE_NAME);
                perror("Error receiving message");
                return 1;
            }
        }

        // Bearbeite Druckauftrag (nur wenn mq_receive() nicht durch ein Signal unterbrochen wurde)
        if (!signalReceived) {
            Content_t* receivedMsg = (Content_t*)buffer;
            bearbeiteDruckauftrag(receivedMsg);
        }
    }

    printf("Server shutting down. Bye!\n");
    printf("==============================================\n");

    // Gebe Ressourcen wieder frei
    mq_close(serverMailbox);
    mq_unlink(SERVER_QUEUE_NAME);
}


void bearbeiteDruckauftrag(Content_t* msg) {
    printf("==============================================\n");
    printf("Druckauftrag '%s' wird bearbeitet...\n", msg->name);

    // Schlafe eine zufällige Zeit (Simuliert Bearbeitungszeit des Druckauftrags)
    int duration = 1 + rand()%10; // set to 10
    sleep(duration);

    // War der Durchauftrag erfolgreich?
    int success = rand()%10;

    // Öffne Clientmailbox
    struct mq_attr clientAttr;
    mqd_t clientMailbox = connectToMailbox(msg->mailbox, &clientAttr, 0);
    if (clientMailbox == -1) {
        perror("Could not open client mailbox");
        return;
    }

    // Erzeuge Serverantwort
    ServerResponse_t response;
    if(success == 9) {
        snprintf(response.message, 100, "Fehler bei der Bearbeitung des Druckauftrags '%s'!", msg->name);
    } else {
        snprintf(response.message, 100, "Bearbeitung Druckauftrag %s abgeschlossen!", msg->name);
    }
    printf("%s\n", response.message);
    printf("==============================================\n");

    // Sende Serverantwort
    mq_send(clientMailbox, (const char*)&response, clientAttr.mq_msgsize, 1);

    // Schließe Clientmailbox
    mq_close(clientMailbox);
}

