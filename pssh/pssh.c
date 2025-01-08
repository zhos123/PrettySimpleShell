#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <readline/readline.h>

#include "builtin.h"
#include "parse.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

Job *jobArr[100];
int w;
int last = 0;
int increment = 0;
int isFG = 1;



void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* **returns** a string used to build the prompt
 * (DO NOT JUST printf() IN HERE!)
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
char *build_prompt ()
{
    char cd[256];
    if (getcwd(cd, 256) == NULL){
        fprintf(stderr,"getcwd() error\n");
        return EXIT_FAILURE;
    }
    char *pthway;
    pthway = malloc(256);
    strcpy(pthway,cd);
    strcat(pthway,"$ ");
    return pthway;
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX-1);
        strncat (probe, "/", PATH_MAX-1);
        strncat (probe, cmd, PATH_MAX-1);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

void handler(int sig){
    pid_t chld;
    int status;
    void (*sav)(int sig);
    int check = 1;
    int breakCheck = 0;
    pid_t fg;
    switch (sig) {
    case SIGCHLD:
        while( (chld = waitpid(-1, &status, WNOHANG | WCONTINUED | WUNTRACED)) > 0) {
            if (chld == tcgetpgrp(STDIN_FILENO)) isFG = 0;
            sav = signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(STDOUT_FILENO, getpgrp());
            signal(SIGTTOU, sav);
            breakCheck = 0;
            for(increment = 0; increment < 10; increment++){
                if(jobArr[increment] == NULL) continue;
                for(int q = 0; q < jobArr[increment]->npids; q++){
                    if(jobArr[increment]->pids[q] == chld){
                        breakCheck = 1; 
                        break;
                    }
                }
                if(breakCheck) break;
            }
            if (WIFCONTINUED(status)) {
                jobArr[increment]->status = BG;
                if(jobArr[increment]->isFG) jobArr[increment]->status = FG;
                printf("[%d] + continued   %s\n",increment, jobArr[increment]->name);
            } 
            else if (WIFSTOPPED(status)) {
                if(chld == jobArr[increment]->pgid)
                    printf("\n[%d] + stopped   %s\n",increment, jobArr[increment]->name);
                last = 0;
                jobArr[increment]->status = STOPPED;
                jobArr[increment]->isFG = false;
            } 
            else {
                last++;
                if(last == jobArr[increment]->npids){
                    char cd[256];
                    getcwd(cd,256);
                    if(!jobArr[increment]->isFG) {
                        printf("\n[%d] + done   %s\n",increment, jobArr[increment]->name);
                    }
                    free(jobArr[increment]->name);
                    free(jobArr[increment]->pids);
                    free(jobArr[increment]);
                    jobArr[increment] = NULL;
                    last = 0;
                }
            }
        }
        break;
    default:
        break;
    }
}

void sighandler(int sig){
    printf("SIGTTOU\n");
    exit(EXIT_FAILURE);  
}

void ifile(Parse *P){
    if(!(P->infile == NULL)){
        int f = open(P->infile, O_RDONLY);
        if(dup2(f, STDIN_FILENO) == -1) {
            fprintf(stderr, "dup2() failed!\n");
            exit(EXIT_FAILURE);
        }
    }
}

void ofile(Parse *P){
    if(!(P->outfile == NULL)){
        int d = creat(P->outfile, 0666);
        if (dup2(d, STDOUT_FILENO) == -1) {
            fprintf(stderr, "dup2() failed!\n");
            exit(EXIT_FAILURE);
        }
    }
}

void execute_input(Parse *P){
    signal(SIGCHLD, handler);
    size_t size;
    Task T;
    int fd[P->ntasks][2];
    pid_t pid[P->ntasks];
    void (*sav)(int sig);
    pid_t *pidArr;
    pidArr = malloc(P->ntasks * sizeof(pid_t));
    for(int j = 0; j < (P->ntasks - 1); j++){
        if (pipe(fd[j]) == -1) {
            fprintf(stderr, "failed to create pipe\n");
            return;
        }
    }
    for(int k = 0; k < P->ntasks; k++){
        T = P->tasks[k];
        if(!(command_found (T.cmd))) return;
        pid[k] = vfork();
        setpgid(pid[k], pid[0]);

        if (pid[k] < 0){
            printf("Failed to fork\n");
            exit(EXIT_FAILURE);
        }
        if (pid[k] == 0){
            if(getpgid(getpid()) == getpid() && !(P->background)){
                sav = signal(SIGTTOU, SIG_IGN);
                tcsetpgrp(STDOUT_FILENO, getpgrp());
                signal(SIGTTOU, sav);
            }
            
            if(k == 0){
                ifile(P);
            }
            else if(dup2(fd[k-1][0], STDIN_FILENO) == -1) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
            }
            if(k == P->ntasks - 1){
                ofile(P);
            }
            else if(dup2(fd[k][1], STDOUT_FILENO) == -1) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
            }
            for(int l = 0; l < P->ntasks-1; l++){
                close(fd[l][0]);
                close(fd[l][1]);
            }
            execvp(T.cmd, T.argv);
            printf("Failed to exec\n");
            exit(EXIT_FAILURE);
        }
    }
    for(int m = 0; m < P->ntasks-1; m++){
        close(fd[m][0]);
        close(fd[m][1]);
    }
    for(int x = 0; x < P->ntasks; x++){
        pidArr[x] = pid[x];
    }
    jobArr[w]->pids = pidArr;
    jobArr[w]->pgid = pid[0];
    jobArr[w]->npids = P->ntasks;
    jobArr[w]->status = BG;
    jobArr[w]->isFG = false;
    if(!(P->background)){
        jobArr[w]->status = FG;
        jobArr[w]->isFG = true;
        pause();
    }

    if(P->background){
        printf("[%d] ", w);
        for(int x = 0; x < P->ntasks; x++){
            printf("%d ",pid[x]);
        }
        printf("\n");
    }

}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse *P)
{
    unsigned int t;
    int check = 0;

    for (t = 0; t < P->ntasks; t++) {
        if (is_builtin (P->tasks[t].cmd)) {
            if(P->infile != NULL || P->outfile != NULL){
                pid_t pid;
                pid = fork();
                if(pid < 0){
                    printf("Failed to fork\n");
                    exit(EXIT_FAILURE);
                }
                if(pid > 0)
                    wait(NULL);  
                else{
                    ifile(P);
                    ofile(P);
                    builtin_execute (P->tasks[t],jobArr);
                    exit(EXIT_SUCCESS);
                }
            }
            else
                if(!strcmp("exit",P->tasks[t].cmd)){
                    for(int y = 0; y < 100; y++){
                        if(jobArr[y]){
                            free(jobArr[y]->name);
                            free(jobArr[y]->pids);
                            free(jobArr[y]);
                        }
                    }
                }
                builtin_execute (P->tasks[t],jobArr);
                if (getpgrp() != tcgetpgrp(STDIN_FILENO)) pause();
        }
        else if (command_found (P->tasks[t].cmd)) {
            if(check)continue;
            w = 0;
            while(jobArr[w]) w++;
            Job *tempJob;
            tempJob = malloc(sizeof(Job));
            jobArr[w] = tempJob;
            char name[2048] = "";
            int e;
            for(int p = 0; p < P->ntasks; p++){
                e = 0;
                while(P->tasks[p].argv[e]){
                    strcat(name,P->tasks[p].argv[e]);
                    strcat(name," ");
                    e++;
                }
                if(p + 1 == P->ntasks) break;
                strcat(name,"| ");
            }
            strcat(name,"\0");
            char *copy = strdup(name);
            memset(name, '\0', 2048);
            jobArr[w]->name = copy;
            execute_input(P);
            check = 1;
        }
        else {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            break;
        }
    }
}


int main (int argc, char** argv)
{
    signal(SIGTTOU, sighandler);
    signal(SIGTTIN, sighandler);
    signal(SIGSTOP, sighandler);
    char* cmdline;
    Parse* P;

    print_banner ();
    char *path;
    path = build_prompt();

    while (1) {
        cmdline = readline (path);
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif

        execute_tasks (P);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
    free(path);
}
