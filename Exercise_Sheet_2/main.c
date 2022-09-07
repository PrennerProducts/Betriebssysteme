#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>


int exitvalue = 0;

int main(int argc, char* argv[]) {



    while (!exitvalue){
        //Shel Prompt
        printf(">");

        // read userinput from console
        char buff [256];
        if (!fgets(buff, 256, stdin)){
            continue;
        }

        buff[strlen(buff)-1] = '\0';

        // exit query
        if(strcmp(buff, "exit")== 0 || strcmp(buff, "EXIT")==0 || strcmp(buff, "Exit")==0){
            exitvalue = 1;
            break;
        }

        // userinput mit strtok pharsen
        char* inputstring [256] = {'\0'};
        char delimiter[] = " ";
        char* argument;
        int count = 1;

        argument = strtok(buff, delimiter);
        inputstring[0]= argument;
        printf("mein Inputstring[0:%s\n", inputstring[0]);

        while(argument != NULL && count < 255){
            inputstring[count]= argument;
            printf("mein Inputstring[%d]: %s\n",count, inputstring[count]);
            argument = strtok(NULL, delimiter);
           count++;
        }

        // check Shebang to identify any script files
        int isSkript = 0;

        FILE* myFileHandle = fopen(inputstring[1],  "r");

        if(myFileHandle == NULL){
            perror("Das Programm konnte nicht geöffnet werden!");
            continue;
        }

        char firstLine[256];
        fgets(firstLine, 254, myFileHandle);
        int firstLineCount = strlen(firstLine);

        if(firstLineCount >= 4 && firstLine[0] == '#' && firstLine[1]== '!'){
            isSkript = 1;
             // Zeilenumbruch am ende entfernen das Posix das nicht mag!
             firstLine[firstLineCount-1] = '\0';


            for ( char* interpreter = firstLine +2; *interpreter < firstLineCount; interpreter++ ) {
                inputstring[0] = interpreter;
                printf("Der Interpreter ist: %s", interpreter);

            }
        }
        fclose(myFileHandle);
       // inputstring[-1]= '\0';


        // Prozess erzeugen mit fork()

        pid_t pid = fork();

        if(pid < 0){
            perror("Error! Konnte nicht Forken!");
            continue;
        }
        else if(pid == 0){
            printf("Das ist  der Kindprozess!\n");
            fflush(stdout);
            if(isSkript == 1){
                printf("das ist ein Skript btw :D\n");
                printf("Inputsting[0] = Interpreter = %s", inputstring[1]);
                execv(inputstring[0], inputstring);
                sleep(10);
            } else if(isSkript == 0){
                printf("das ist Kein Skript \n");
                printf("das inputstring+1 %s  \n", *inputstring+1);
                execv(inputstring[1], inputstring+1);
                sleep(10);
            }
            perror("Fehler bei execv()");
            exit(-42);

        }

        else if (pid  > 0){
            printf("Das ist der Elternprozess \n");
            waitpid(pid, NULL, 0);
            printf("Der Kindprozess hat sich beendet!");
        }



    }


    return 0;
}
