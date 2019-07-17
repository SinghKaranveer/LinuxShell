#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "builtin.h"
#include "parse.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0
#define MAX_JOBS 100
typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct{
    char name[80];
    pid_t* pids;
    int numPids;
    unsigned int npids;
    JobStatus status;
    int running;
    pid_t pgid;
    int arrayIndex;
} Job;

static Job jobList[MAX_JOBS];
static int numOfJobs = 0;

void setForeground(pid_t pgid)
{
    void (*old)(int);
    old = signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(STDIN_FILENO, pgid);
    tcsetpgrp(STDOUT_FILENO, pgid);
    signal(SIGTTOU, old);
}

int initJob(int npids)
{
    int i = 0;
    while(jobList[i].npids != 0)
    {
        if(i >= MAX_JOBS)
        {
            fprintf(stderr, "Error, maximum number of Jobs reached\n");
            return -1;
        }
        i++;
    }
    jobList[i].npids = npids;
    jobList[i].status = FG;
    jobList[i].numPids = 0;
    jobList[i].running = 1;
    jobList[i].arrayIndex = 0;
    jobList[i].pids = malloc(jobList[i].npids * sizeof(pid_t));
    return i;
}

void hello(void)
{
    printf("hello");
}
void addPid(int index, pid_t pid)
{   
    int i = jobList[index].arrayIndex;
    jobList[index].pids[i] = pid;
    jobList[index].arrayIndex++; 
}


void addJob(char* name, pid_t pgid, pid_t* pid, int index, JobStatus status)
{
    int i = index;
    strcpy(jobList[i].name, name);
    jobList[i].status = status;
    jobList[i].pgid = pgid;
    jobList[i].running = 1;
    numOfJobs++;
}
void jobs(void)
{
    int i;
    char stat[8];
    for(i = 0; i < numOfJobs; i++)
    {
        if(jobList[i].running == 1 && jobList[i].status != TERM)
        {
            if(jobList[i].status == STOPPED)
            {
                strcpy(stat, "stopped");
            }
            else
            {
                strcpy(stat, "running");
            }
            printf("[%i] + %s   %s\n", i, stat, jobList[i].name); 
        }
    }
}

void fg(char* job)
{
    int i;
    if(job == NULL)
    {
        printf("pssh: invalid job number: \n");
        return;
    }
    if(job[0] == '%')
    {
        job++;
        i = atoi(job);
        if(jobList[i].running == 1)
        {
            setForeground(jobList[i].pgid);
            kill(-1*jobList[i].pgid, SIGCONT);
            return;
        }
        else
        {
            printf("pssh: invalid job number: %i\n",i);
            return;
        }

        printf("%i\n",i);
    }
    else
    {
        printf("pssh: invalid job number: %s\n",job);
        return;
    }
}
int checkPid(pid_t pid)
{
    if(kill(pid, 0) == 0)
    {
        return 1;
    }
    else if(errno == EPERM)
    {
        return 0;
    }
    else if(errno == ESRCH)
    {
        return 0;
    }
    return 0;
}

void builtinKill(Task T)
{
    int signal, i;
    char* job;
    if (T.argv[1] == NULL)
    {
        printf("Usage: kill [-s <signal>] <pid> | %%<job> ...\n");
        return;
    }
    else if(!strcmp(T.argv[1], "-s"))
    {
        if(T.argv[2] == NULL)
        {
            printf("Usage: kill [-s <signal>] <pid> | %%<job> ...\n");
            return;
        }
        signal = atoi(T.argv[2]);
        i = 3;
    }
    else
    {
        signal = SIGINT;
        i = 1;
    }
    while(T.argv[i] != NULL)
    {
        if(T.argv[i][0] == '%')//JOB
        {
            job = T.argv[i];
            job++;
            int index = atoi(job);
            if(jobList[index].status != TERM)
            {
                kill(jobList[index].pgid*-1, signal);
            }
            else
            {
                printf("pssh: invalid job number: %i\n", index);
            }

            //printf("JOB: %s\n", T.argv[i]++);
        }
        else
        {
            if(checkPid(atoi(T.argv[i])))
            {
                kill(atoi(T.argv[i]), signal);
            }
            else
            {
                printf("pssh: invalid pid: %s\n",T.argv[i]);
                return;
            }
        }
        i++;
    }
    return;
}

void bg(char* job)
{
    int i;
    if(job == NULL)
    {
        printf("pssh: invalid job number: \n");
        return;
    }
    if(job[0] == '%')
    {
        job++;
        i = atoi(job);
        if(jobList[i].running == 1)
        {
            kill(-1*jobList[i].pgid,SIGCONT);
            printf("[%i] + continued %s\n",i,jobList[i].name);
            jobList[i].status = BG;
            return;
        }
        else
        {
            printf("pssh: invalid job number: %i\n",i);
            return;
        }

        printf("%i\n",i);
    }
    else
    {
        printf("pssh: invalid job number: %s\n",job);
        return;
    }
}

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
    char* cwd = malloc(sizeof(char) * (PATH_MAX+2));
    getcwd(cwd, PATH_MAX);
    strcat(cwd, "$ ");
    return cwd;
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

        strncpy (probe, dir, PATH_MAX);
        strncat (probe, "/", PATH_MAX);
        strncat (probe, cmd, PATH_MAX);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}



static int searchPid(pid_t pid)
{
    int j = 0;
    int i = -1;
    for(i = 0; i < MAX_JOBS; i++)
    {
        if(jobList[i].running == 1)
        {
            for(j = 0; j < jobList[i].npids; j++)
            {
                if(jobList[i].pids[j] == pid)
                    return i;
            }
        }
    }
    return 10;
}

void realClean(int i)
{
    jobList[i].running = 0;
    jobList[i].pids = 0;
    jobList[i].arrayIndex = 0;
    jobList[i].npids = 0;
    jobList[i].numPids = 0;
    jobList[i].status = TERM;
}
void cleanJob(int i)
{
    jobList[i].numPids++;
    if(jobList[i].numPids >= jobList[i].npids)
    {
        jobList[i].running = 0;
        jobList[i].pids = 0;
        jobList[i].arrayIndex = 0;
        jobList[i].npids = 0;
        jobList[i].numPids = 0;
        jobList[i].status = TERM;
    }
}

static void handler(int sig)
{
    int jobListIndex;
    int status;
    int background = 0;
    if(sig == SIGCHLD)
    {
        pid_t child;
        while ((child = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) 
        {
            if(WSTOPSIG(status) == SIGTSTP)
            {
                setForeground(getpgrp());
                jobListIndex = searchPid(child);        
                jobList[jobListIndex].status = STOPPED;
                kill(-1*child, SIGTSTP);
                printf("[%i] + suspended      %s\n",jobListIndex,jobList[jobListIndex].name);
            }
            else if(WIFCONTINUED(status))
            {
                kill(-1*child, SIGCONT);
            }
            else
            {
                jobListIndex = searchPid(child);        
                if(!checkPid(child))
                {
                    if(jobList[jobListIndex].status == BG)
                    {
                        background = 1;

                    }
                    cleanJob(jobListIndex);
                    if(jobList[jobListIndex].running == 0)
                    {
                        setForeground(getpgrp());
                        if(background)
                            printf("\n[%i] + done     %s\n",jobListIndex, jobList[jobListIndex].name);
                    
                    }
                }
                else
                {
                    jobList[jobListIndex].status = STOPPED;
                }
            }
        }

    }
    if(sig == SIGINT)
    {
        pid_t pid = tcgetpgrp(0);
        kill(-1*pid, SIGINT);

    }
    if(sig == SIGTSTP)
    {
        printf("SIGTSTP\n");
    }
}

void handler_sigttou(int sig)
{
    while (tcgetpgrp(STDOUT_FILENO) != getpid ())
        pause ();
}


/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P, char* cmdLine)
{
    unsigned int t,i;
    pid_t pid;
    pid_t groupLeaderPid;
    int fd[2];
    //Job currentJob;
    int in = STDIN_FILENO;
    int out = STDOUT_FILENO;
    int closePipe = 0;
    int index;
    char commandLine[200];
    signal(SIGCHLD, handler);
    pid_t pidArray[P->ntasks];
    strcpy(commandLine, "");
    for (t = 0; t < P->ntasks; t++) 
    {
        i = 0;
        while(P->tasks[t].argv[i] != NULL)  //Recreate command prompt for Jobs Struct
        {
            strcat(commandLine, P->tasks[t].argv[i]);
            strcat(commandLine, " ");
            i++;
        }
        if (is_builtin (P->tasks[t].cmd)) {
            builtin_execute (P->tasks[t], P->infile, P->outfile);
        }
        else if (!strcmp (P->tasks[t].cmd, "jobs"))
        {
            jobs();
            strcpy(commandLine, "");
            return;
        }
        else if (!strcmp (P->tasks[t].cmd, "bg"))
        {
            bg(P->tasks[t].argv[1]);
            strcpy(commandLine, "");
            return;
        }
        else if (!strcmp (P->tasks[t].cmd, "fg"))
        {
            fg(P->tasks[t].argv[1]);
            strcpy(commandLine, "");
            return;
        }
        else if (!strcmp (P->tasks[t].cmd, "kill"))
        {
            builtinKill(P->tasks[t]);
            strcpy(commandLine, "");
            return;
        }
        else if (command_found (P->tasks[t].cmd)) {
            if(t == 0)
                index = initJob(P->ntasks);          
            if(P->ntasks == 1)
            {
                pid = vfork();
                if(pid > 0)
                {
                    pidArray[t] = pid;
                    groupLeaderPid = pid;
                 }
                setpgid(pid,pid);  //Child and Parent will try to place child in own process group
            }
            else
            {
                if(t == P->ntasks-1)
                {
                    out = STDOUT_FILENO;
                    in = fd[0];
                    close(fd[1]);
                    pid = vfork();
                    if(pid > 0)
                    {
                        pidArray[t] = pid;
                    }
                    closePipe = 1;
                }
                else
                {
                    pipe(fd);
                    strcat(commandLine, "| ");
                    out = fd[1];
                    pid = vfork();
                    if(t == 0 && pid > 0)
                    {
                        groupLeaderPid = pid;
                    }
                    setpgid(pid,groupLeaderPid);
                    pidArray[t] = pid;
                    closePipe = 1;
    
                }
            }

            if (pid < 0) 
            { /* error occured */
                fprintf(stderr, "Fork failed");
            }
  
            else if (pid == 0) /* child process */ 
            {
                addPid(index, getpid());

                if(t == 0)
                {
                    setForeground(getpid()); 
                }
                setpgid(getpid(),groupLeaderPid);
                if(P->infile && t == 0)
                {
                    in = open(P->infile, O_RDONLY);
                    closePipe = 1;
                    strcat(commandLine, "< ");
                    strcat(commandLine, P->infile);
                }
                if(P->outfile && t == P->ntasks-1)
                {
                    out = open(P->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    closePipe = 0;
                    strcat(commandLine, "> ");
                    strcat(commandLine, P->outfile);
                    if(dup2(out, STDOUT_FILENO) < 0)
                        fprintf(stderr, "Error: could not update file descriptor");
                }    
            
                
                    if (in != STDIN_FILENO) 
                    {
                        dup2(in, STDIN_FILENO);
                    }
                    
                    if (out != STDOUT_FILENO)
                    {
                        dup2(out, STDOUT_FILENO);
                    }
                    
                    if(closePipe == 1)
                    {
                        close(fd[0]);
                        closePipe = 0;
                    }   
                signal(SIGCHLD, SIG_DFL);
                signal(SIGTTOU, SIG_DFL);
                execvp(P->tasks[t].argv[0], P->tasks[t].argv);
            }
            else /* parent process */
            { 
            
                /* parent will wait for the child to complete */
                if(closePipe == 1)
                {
                    close(fd[1]);
                    in = fd[0];
                }
                
            } 
            addJob(commandLine,groupLeaderPid, pidArray, index, FG);
        }
        else {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            break;
        }
    }
    if(P->background == 0)
    {
       // for(t = 0; t < P->ntasks && tcgetpgrp(STDOUT_FILENO) != getpid (); t++)
       // {
       //     printf("HELLO\n");

    }
    else
    {
        setForeground(getpgrp());
        jobList[index].status = BG;
        printf("[%i] ", searchPid(groupLeaderPid));
        for(t = 0; t < P->ntasks; t++)
        {
            printf("%i ", pidArray[t]);
        }
        printf("\n");
    }

}
int main (int argc, char** argv)
{
    signal (SIGTTOU, handler_sigttou);
    signal (SIGTTIN, handler_sigttou);
    char* cmdline;
    Parse* P;


    int i;  
    for(i = 0; i < MAX_JOBS; i++)
    {       
        jobList[i].running = 0;
        jobList[i].npids = 0;
        jobList[i].numPids = 0;
        jobList[i].status = TERM;
    }


    print_banner ();

    while (1) {
        cmdline = readline (build_prompt());
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

        execute_tasks (P,cmdline);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
