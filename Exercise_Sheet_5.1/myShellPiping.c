
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


int main(int argc, char* argv[]) {

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
        // daher ersetzen ich das '\n' durch das Null-Terminierungssymbol
        input[strlen(input) - 1] = '\0';

        // Wenn die Benutzereingabe "exit" ist, dann beende die Endlosschleife und damit die Schell
        if (strcmp(input, "exit") == 0) {
            break;
        }

        // Komandos aufsplitten mit strtock()

        char *commands[42];
        int commandCount = 0;

        char *inputToken = strtok(input, "|");
        while (inputToken != NULL) {
            commands[commandCount] = inputToken;
            commandCount++;
            inputToken = strtok(NULL, "|");
        }

        int pipefdfromlastiteration;
        pid_t childpids[42];
        int childcount = 0;

        for (int i = 0; i < commandCount; ++i) {

            // Dieses Array speichert die Tokens bzw. Argumente der Benutzereingabe
            // Wir gehen davon aus, dass es nicht mehr als 127 Tokens gibt
            // Wir brauchen noch Platz für das terminierende NULL, daher muss unser Array 128 Einträge haben
            char *arguments[128];

            // Zerteile die Benutzereingabe mithilfe von strtok in Tokens (mit Trennzeichen " ")
            int j = 1; // Wir starten den Index bei 1, damit wir noch Platz im Falle einer Scriptdatei noch Platz für den Interpreter haben
            char *token = strtok(commands[i], " "); // Dem ersten Aufruf von strtok() wird der aufzuteilende String übergeben
            // Solange strtok() einen gültigen Token zurückgegeben hat und es noch weniger als 127 Token gibt
            while (token != NULL && j < 127) {
                arguments[j] = token; // Speichere das Token im Argument-Array
                token = strtok(NULL, " ");  // Allen weiteren Aufrufen von strtok() wird NULL übergeben
                j++;
            }
            arguments[j] = NULL; // Damit execv() weiß, wann die Argumentenlisten zu Ende ist, müssen wir am Ende noch ein NULL schreiben


            // Nun müssen wir noch überprüfen, ob es sich um eine Skriptdatei handelt
            // Dazu versuchen wir den als erstes Token übergebene Programm zu öffnen
            int isScriptFile = 0; // Hier speichern wir, ob es sich um eine Skriptdatei handelt
            FILE *file;
            file = fopen(arguments[1], "r");
            if (file == NULL) {
                // Programm konne nicht geöffnet werden, daher geben wir eine Fehlermeldung aus
                perror("Programm konnte nicht geöffnet werden");
                // und unterbrechen die aktuelle Iteration und starten eine neue
                continue;
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
                char *interpreter;
                for (interpreter = firstLine + 2; interpreter[0] == ' '; interpreter++);
                // Im Argumenten-Array müssen wir an die allererste Stelle einen Zeiger auf den Interpreter speichern
                arguments[0] = interpreter;
            }

            // Nicht vergessen, die Datei wieder zu schließen
            fclose(file);

            int pipefd[2];
            if(commandCount > 1 && i < commandCount -1){
                if(pipe(pipefd) == -1){
                    perror("Es konnte keine Pipe erstellt werden!");
                    exit(-31);
                }
            }

            // Nun rufen wir fork() auf, um einen neuen Prozess zu erzeugen
            pid_t pid = fork();
            if (pid < -1) { // Fehlerfall
                perror("Fehler bei fork()");
                continue;
            } else if (pid == 0) { // Kindprozess

                // STDIN umleiten
                if(i > 0){
                    if(dup2(pipefdfromlastiteration, STDIN_FILENO)== -1){
                        perror("Fehler bei dupt2!");
                        exit(13);
                    }
                    close(pipefdfromlastiteration);
                }

                // STDOUT umleiten
                if (i < commandCount -1){
                    close(pipefd[0]);
                    if(dup2(pipefd[1], STDIN_FILENO)== -1){
                        perror("Fehler bei dupt2!");
                        exit(14);
                    }
                    close(pipefd[1]);
                }

                // Der Kindprozess ruft exec() auf
                // je nachdem, ob es sich um eine Skriptdatei handelt, beginnen die Argumente an der Stelle 0 oder 1
                if (isScriptFile) {
                    execv(arguments[0], arguments);
                } else {
                    execv(arguments[1], arguments + 1);
                }
                perror("Fehler bei exec()");   // Wenn diese Stelle erreicht wird, dann hat exec() nicht richtig funktioniert
                // Hier ist unbedingt ein exit() notwendig, da sonst im Fehlerfall der Kindprozess die Shell weiter ausführt und der Elternprozess weiter wartet
                exit(1);
            } else { // Elternprozess
                if(i > 0){
                    close(pipefd[1]);
                }
                if(commandCount > 0 && i < commandCount -1){
                    close(pipefd[1]);
                    pipefdfromlastiteration = pipefd[0];
                }
                childpids[childcount] = pid;
                childcount++;
            }
        } // end for
        for (int i = 0; i < childcount; ++i) {
            waitpid(childpids[i], NULL, 0);
        }
    } // end while

    return 0;
}
