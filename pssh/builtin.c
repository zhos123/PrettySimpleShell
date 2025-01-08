#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#include "builtin.h"
#include "parse.h"

const char *status_strings[] = {
    "stopped",
    "stopped",
    "running",
    "running",
};

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    "jobs",
    "fg",
    "bg",
    "kill",
    NULL
};

const char *sigabbrev(unsigned int sig){

    const char *sigs[31] = { "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT",
    "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM",
    "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN",
    "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO",
    "PWR", "SYS" };
    if (sig == 0 || sig > 31)
        return NULL;
    return sigs[sig-1];
}

int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}


void builtin_execute (Task T, Job *arr[])
{
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    }
    else if (!strcmp (T.cmd, "which")){
        char* dir;
        char* tmp;
        char* path;
        char* state;
        char probe[PATH_MAX];
        path = strdup(getenv("PATH"));
        if(T.argv[1] == NULL){
            return;
        }
        else if(is_builtin(T.argv[1])){
            printf("%s: shell built-in command\n",T.argv[1]);
            return;
        }

        for (tmp=path; ; tmp=NULL) {
            dir = strtok_r (tmp, ":", &state);
            if (!dir)
                break;
            strncpy (probe, dir, PATH_MAX-1);
            strncat (probe, "/", PATH_MAX-1);
            strncat (probe, T.argv[1], PATH_MAX-1);

            if (access (probe, X_OK) == 0) {
                printf("%s\n",probe);
                break;
            }
        }
        free(path);
    }
    else if(!strcmp (T.cmd, "jobs")){
        for(int j = 0; j < 100; j++){
            if(arr[j]){
                printf("[%d] + %s   %s\n",j,status_strings[arr[j]->status],arr[j]->name);
            }
        }
    }
    else if(!strcmp (T.cmd, "fg")){
        int numArgs = 0;    
        while(T.argv[numArgs]) numArgs++;
        if(numArgs != 2){
            printf("Usage: fg %%<job number>\n");
            return;
        }
        void (*sav)(int sig);
        sav = signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDOUT_FILENO, getpgrp());
        signal(SIGTTOU, sav);
        char c;
        c = T.argv[1][1];
        c -= 48;
        if(arr[c] == NULL){
            printf("pssh: invalid job number: [%d]\n",c);
            return;
        }
        tcsetpgrp(STDOUT_FILENO, arr[c]->pgid);
        arr[c]->isFG = true;
        if(arr[c]->status == STOPPED) {
            for(int h = 0; h < arr[c]->npids; h++){
                kill(arr[c]->pids[h], SIGCONT);
            }
        }
    }
    else if(!strcmp (T.cmd, "bg")){
        int numArgs = 0;    
        while(T.argv[numArgs]) numArgs++;
        
        if(numArgs != 2){
            printf("Usage: bg %%<job number>\n");
            return;
        }
        char c;
        c = T.argv[1][1];
        c -= 48;
        if(arr[c] == NULL){
            printf("pssh: invalid job number: [%d]\n",c);
            return;
        }
        arr[c]->status = BG;
        for(int r = 0; r < arr[c]->npids; r++){
            kill(arr[c]->pids[r], SIGCONT);
        }
        kill(getpgrp(), SIGCHLD);
    }
    else if(!strcmp (T.cmd, "kill")){
        int numArgs = 0;
        int signal = 15;
        
        while(T.argv[numArgs]) numArgs++;
        
        if(numArgs == 2){
            if(T.argv[1][0] == '%'){
                char c;
                c = T.argv[1][1];
                c -= 48;
                if(arr[c] == NULL){
                    printf("pssh: invalid job number: [%d]\n",c);
                    return;
                }
                for(int t = 0; t < arr[c]->npids; t++){
                    kill(arr[c]->pids[t], signal);
                }
            }
            else{
                if(!(kill(T.argv[1], 0))){
                    printf("pssh: invalid pid: [%s]\n",T.argv[1]);
                }
                kill(atoi(T.argv[1]), signal);
            }
            kill(getpgrp(), SIGCHLD);
            return;
        }
        if(!strcmp (T.argv[1], "-s")){
            signal = atoi(T.argv[2]);
        }

        if(T.argv[3][0] == '%'){
            char c;
            c = T.argv[3][1];
            c -= 48;
            if(arr[c] == NULL){
                printf("pssh: invalid job number: [%d]\n",c);
                return;
            }
            for(int t = 0; t < arr[c]->npids; t++){
                kill(arr[c]->pids[t], signal);
            }
        }
        else{
            if(!(kill(T.argv[1], 0))){
                printf("pssh: invalid pid: [%s]\n",T.argv[1]);
                return;
            }
            kill(atoi(T.argv[3]), signal);
        }
        kill(getpgrp(), SIGCHLD);
    }
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
