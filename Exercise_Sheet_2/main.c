#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>


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
        char* inputstring [256];
        char delimiter[] = " ";
        char* argument;
        int count = 1;

        argument = strtok(buff, delimiter);
        inputstring[0]= argument;

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

        if(firstLineCount >= 4 && firstLine[0] == '#' && firstLine[2]== '!'){
            isSkript = 1;
             // Zeilenumbruch am ende entfernen das Posix das nicht mag!
             firstLine[firstLineCount-1] = '\0';
            /// DA FEHLT NO INTERPRETER
        }
        fclose(myFileHandle);


        // Prozess erzeugen mit fork()

        pid_t cpid = fork();

        if(cpid < 0){
            perror("Error! Konnte nicht Forken!");
            continue;
        }
        else if(cpid == 0){
            printf("Das ist  der Kindprozess!\n");
        }
        else if (cpid  > 0){
            printf("Das ist der Elternprozess \n");
        }













    }


    return 0;
}
