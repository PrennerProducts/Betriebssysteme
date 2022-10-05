
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>




// Folgende Variablen sind deswegen global, damit der Signal-Handler darauf zugreifen kann

// Speichert die Anzahl der Kindprozesse
int childcount = 0;

// Speichert die Pids der Kindprozesse, die die einzelnen Befehle ausführen
pid_t childpids[32];


// Das ist der Signal-Handler, der für den graceful shutdown zuständig ist
void graceful_shutdown_handler(int signum) {
    if (childcount == 0) {
        printf("\nExit myShell? (y/n): ");
        char buffer[128];
        fgets(buffer, 128, stdin);
        if (strncmp(buffer, "y", 1) == 0) {
            exit(0);
        }
    } else {
        // Das empfangene Signal an alle Kinder schicken
        for (int i = 0; i < childcount; i++) {
            kill(childpids[i], signum);
        }
    }
}


int main(int argc, char* argv[]) {

    // Erzeuge die sigaction Struktur
    struct sigaction my_signal;

    // Befülle die sigaction Struktur
    my_signal.sa_handler = graceful_shutdown_handler;
    // Wenn der Signal-Handler gerade aktiv ist, sollten alle anderen Signale,
    // die auch den Prozess beenden, ignoriert werden
    sigemptyset (&my_signal.sa_mask);
    sigaddset(&my_signal.sa_mask, SIGTERM);
    sigaddset(&my_signal.sa_mask, SIGINT);
    sigaddset(&my_signal.sa_mask, SIGQUIT);
    // Hier sollte man nicht SA_RESTART angeben, da wir ja im Signal-Handler fgets() aufrufen,
    // und wenn der fgets()-Aufruf in der main() neu gestartet wird, haben wir ein Problem.
    my_signal.sa_flags = 0;

    // Registriere den Signal-Handler für die Signale SIGTERM, SIGINT und SIGQUIT
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

    while(1) {

        // Shell Prompt wird ausgegeben
        printf("> ");

        // Lese Benutzereingabe ein
        char input[1024];
        if (!fgets(input, 1024, stdin)) {
            // fgets war nicht erfolgreich (d.h. Returnwert ist NULL), daher unterbreche die derzeitige Iteration und starte eine neue Iteration
            continue;
        }

        // Die Benutzereingabe enthält als letztes Zeichen immer '\n', was aber bei der Weiterverarbeitung stört,
        // daher ersetzen wir das '\n' durch das Null-Terminierungssymbol
        input[strlen(input) -1] = '\0';

        // Wenn die Benutzereingabe "exit" ist, dann beende die Endlosschleife und damit die Schell
        if(strcmp(input, "exit") == 0) {
            break;
        }

        // Dieses Array speichert die Tokens bzw. einzenlen Befehle der Benutzereingabe
        // Wir gehen davon aus, dass es nicht mehr als 32 Befehle gibt
        char* commands[32];

        // Speichert die Anzahl der Befehle
        int commandcount = 0;

        // Zerteile die Benutzereingabe mithilfe von strtok in die einzelnen Befehle (mit Trennzeichen "|")
        char* ptoken = strtok(input, "|");
        // Solange strtok() einen gültigen Token zurückgegeben hat und es noch weniger als 32 Token gibt
        while (ptoken != NULL && commandcount < 32) {
            commands[commandcount] = ptoken; // Speichere das Token im Command-Array
            ptoken = strtok(NULL, "|");  // Allen weiteren Aufrufen von strtok() wird NULL übergeben
            commandcount++;
        }

        // Speichert das Read-Ende der Pipe, das für die nächste Iteration benötigt wird
        int pipefdfromlastiteration;
        // Iteriere über die einzelnen Befehle
        for (int i = 0; i < commandcount; i++) {

            // Dieses Array speichert die einzelnen Argumente des aktuellen Befehls
            // Wir gehen davon aus, dass es nicht mehr als 127 Tokens gibt
            // Wir brauchen noch Platz für das terminierende NULL, daher muss unser Array 128 Einträge haben
            char* arguments[128];

            // Zerteile die Benutzereingabe mithilfe von strtok in Tokens (mit Trennzeichen " ")
            int j = 1; // Wir starten den Index bei 1, damit wir noch Platz im Falle einer Scriptdatei noch Platz für den Interpreter haben
            char* token = strtok(commands[i], " "); // Dem ersten Aufruf von strtok() wird der aufzuteilende String übergeben
            // Solange strtok() einen gültigen Token zurückgegeben hat und es noch weniger als 127 Token gibt
            while(token != NULL && j < 127) {
                arguments[j] = token; // Speichere das Token im Argument-Array
                token = strtok(NULL, " ");  // Allen weiteren Aufrufen von strtok() wird NULL übergeben
                j++;
            }
            arguments[j] = NULL; // Damit execv() weiß, wann die Argumentenlisten zu Ende ist, müssen wir am Ende noch ein NULL schreiben


            // Nun müssen wir noch überprüfen, ob es sich um eine Skriptdatei handelt
            // Dazu versuchen wir den als erstes Token übergebene Programm zu öffnen
            int isScriptFile = 0; // Hier speichern wir, ob es sich um eine Skriptdatei handelt
            FILE* file;
            file = fopen(arguments[1], "r");
            if(file == NULL) {
                // Programm konne nicht geöffnet werden, daher geben wir eine Fehlermeldung aus
                perror("Programm konnte nicht geöffnet werden");
                // Falls noch ein Pipe-Ende offen ist, müssen wir es schließen
                // Schon gestartete Kinder beenden sich normalerweise, wenn wir die Pipe schließen
                if (i > 0) {
                    close(pipefdfromlastiteration);
                }
                // Wir müssen noch commandcount anpassen, damit waitpid nach der for-schleife korrekt funktioniert
                commandcount = i;
                // wir unterbrechen die for-Schleife
                break;
            }

            // Lese die erste Zeile
            char firstLine[256]; // Wir nehmen an, dass die erste Zeile nicht mehr als 255 Zeichen hat.
            fgets(firstLine, 256, file);
            int firstLineCount = strlen(firstLine);

            // Wenn die erste Zeile mindestens 4 Zeichen hat (2 Zeichen für Shebang + mind 1. Zeichen für Interpreter + Zeilenumbruch)
            // und die ersten zwei Zeichen die Shebang ist, dann ist es eine Skriptdatei
            if (firstLineCount >= 4 && firstLine[0] == '#' && firstLine[1] == '!') {
                isScriptFile = 1;
                // Wir müssen wieder den Zeilenumbruch entfernen, da er sonst stört
                firstLine[firstLineCount - 1] = '\0';
                // exec() mag keine Leerzeichen am Anfang eines Pfades, manche Skriptdateien haben aber Leerzeichen drinnen
                // Daher müssen wir jetzt die Leerzeichen vor dem Interpreter überspringen
                //
                // Theoretisch müssten wir das auch mit den Leerzeichen hinter dem Interpreter machen und zusätzlich noch andere Whitespace-Characters wie Tab inkludieren
                // Ausserdem könnten hinter dem Interpreter noch zusätzliche Argumente stehen.
                // Aber das ignorieren wir in dieser Aufgabe
                char* interpreter;
                for(interpreter = firstLine + 2; interpreter[0] == ' '; interpreter++);
                // Im Argumenten-Array müssen wir an die allererste Stelle einen Zeiger auf den Interpreter speichern
                arguments[0] = interpreter;
            }

            // Nicht vergessen, die Datei wieder zu schließen
            fclose(file);


            int pipefd[2]; // In dieses Array schreibt pipe() die Dateideskriptoren der Pipe
            // Erzeuge die Pipe (aber nur wenn mehr als einen Befehl und nicht die allerletzte Iteration)
            if (commandcount > 1 && i < commandcount -1) {
                if (pipe(pipefd) == -1) {
                    perror("Fehler bei pipe()");
                    exit(1); // Nicht die beste Fehlerbehandlungsmethode, aber ist ja nur ein Übungsbeispiel
                }
            }

            // Nun rufen wir fork() auf, um einen neuen Prozess zu erzeugen
            childpids[i] = fork();
            if (childpids[i] < -1) { // Fehlerfall
                perror("Fehler bei fork()");
                exit(1);
            } else if (childpids[i] == 0) { // Kindprozess

                // Leite stdin um (Nur wenn nicht die allererste Iteration der for-schleife)
                if (i > 0) {
                    if (dup2(pipefdfromlastiteration, STDIN_FILENO) == -1) {
                        perror("Fehler bei dup2()");
                        exit(1); // Nicht die beste Fehlerbehandlungsmethode, aber ist ja nur ein Übungsbeispiel
                    }
                    close(pipefdfromlastiteration); // Originaler Dateideskriptor muss nach dup2() geschlossen werden
                }

                //  Leite stdout um (Nur wenn mehr als ein Befehl und nicht die allerletzte Iteration der for-schleife)
                if (commandcount > 1 && i < commandcount -1) {
                    close(pipefd[0]); // Nicht benutzte Pipe-Enden schließen
                    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                        perror("Fehler bei dup2()");
                        exit(1); // Nicht die beste Fehlerbehandlungsmethode, aber ist ja nur ein Übungsbeispiel
                    }
                    close(pipefd[1]); // Originaler Dateideskriptor muss nach dup2() geschlossen werden
                }

                // Der Kindprozess ruft exec() auf
                // je nachdem, ob es sich um eine Skriptdatei handelt, beginnen die Argumente an der Stelle 0 oder 1
                if (isScriptFile) {
                    execv(arguments[0], arguments);
                } else {
                    execv(arguments[1], arguments+1);
                }
                perror("Fehler bei exec()");   // Wenn diese Stelle erreicht wird, dann hat exec() nicht richtig funktioniert
                // Hier ist unbedingt ein exit() notwendig, da sonst im Fehlerfall der Kindprozess die Shell weiter ausführt und der Elternprozess weiter wartet
                exit(1);

            } else { // Elternprozess
                childcount++;
                // Der Elternprozess muss auch nicht benutzte Pipe-Enden schließen
                if (i > 0) {
                    close(pipefdfromlastiteration); // Read-Ende der letzten Iteration schließen
                }
                if (commandcount > 1 && i < commandcount -1) {
                    pipefdfromlastiteration = pipefd[0]; // Das Read-Ende der Pipe wird für die nächste Iteration benötigt, daher noch nicht schließen
                    close(pipefd[1]); // Write-Ende der Pipe schließen
                }
            }

        } // end for

        // Der Elternprozess wartet, bis sich alle Kinder beendet haben
        for (int i = 0; i < childcount; i++) {
            // Wenn waitpid() vom Signal-Handler unterbrochen wird, dann starten wir es einmalig neu
            if (waitpid(childpids[i], 0, 0) == -1) {
                waitpid(childpids[i], 0, 0);
            }
        }
        // Danach childcount auf 0 setzten, damit der Signal-Handler mitbekommt, dass im Moment keine Kinder existieren
        childcount = 0;

    } // end while

}
