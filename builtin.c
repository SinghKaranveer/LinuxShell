#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    NULL
};


int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}


void builtin_execute (Task T,char* in, char* out)
{
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    }
    else if (!strcmp(T.cmd, "which"))//
    {

        char *path;
        char *s;
        //char* command = strdup(T.argv[1]);
        char *p = NULL;
        char buffer[50];
        int outfile; 
        int oldIn = 0;
        int oldOut = 1;
        //FILE *infile;
        path = strdup(getenv("PATH"));
        s = path;
        if (T.argv[1] == NULL)
        {
            return;
        }
        
            if(out)
            {
                outfile = open(out, O_WRONLY|O_CREAT|O_TRUNC, 0644);
                oldOut = dup(STDOUT_FILENO);
                dup2(outfile, STDOUT_FILENO);
            }
        do 
        {

            if(is_builtin(T.argv[1]))
            {
                printf("%s: shell built-in command\n", T.argv[1]);
                break;
            }

            p = strchr(s, ':');
            if (p != NULL)
            {
                p[0] = 0;
            }
            if(T.argv[1] != NULL)
            {
            strcpy(buffer, s);
            strcat(buffer,"/");
            strcat(buffer,T.argv[1]);
            if(access(buffer, X_OK) == 0)
            {
                printf("%s\n",buffer);
                break;
            }
            s = p + 1;
            }
        
        } while (p != NULL);

            if(in)
            {
                dup2(oldIn, STDIN_FILENO);
            }
            if(out)
            {
                dup2(oldOut, 1);
            }

    }
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
