#ifndef _builtin_h_
#define _builtin_h_

#include "parse.h"

typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct {
    char* name;
    pid_t* pids;
    unsigned int npids;
    pid_t pgid;
    JobStatus status;
    bool isFG;
} Job;

int is_builtin (char* cmd);
void builtin_execute (Task T, Job *arr[]);
int builtin_which (Task T);

#endif /* _builtin_h_ */
