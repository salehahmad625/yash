#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <fcntl.h>
#define min(x, y) (((x) < (y)) ? (x) : (y))

int indOfSym(char **arr, char *sym)
{
    int i = 0;
    while (!(arr[i] == NULL))
    {
        if (strcmp(arr[i], sym) == 0)
        {
            return i;
        }
        i++;
    }
    return -1;
}

int numTokens(char *str)
{
    char *token = (char *)malloc(30 * sizeof(char));
    int ind = 0;
    while (token = strtok_r(str, " ", &str))
    {
        ind++;
    }
    free(token);
    return ind;
}

char **parseString(char *str)
{
    char **arrOfChars = (char **)malloc((1000) * sizeof(char *)); //alternatively set big number to wc
    char *token = (char *)malloc(30 * sizeof(char));
    int ind = 0;
    while (token = strtok_r(str, " ", &str))
    {
        arrOfChars[ind] = token;
        ind++;
    }
    return arrOfChars;
}

void sig_handler(int signo)
{
    switch (signo)
    {
    case SIGINT:
        printf("Hi");
        exit(0);
        break;
    case SIGTSTP:
        exit(0);
    }
}

int main(int argc, char **argv)
{
    pid_t cpid;
    char *inString;
    char **parsedcmd;

    char *pipesym = "|";
    char *output = ">";
    char *input = "<";

    signal(SIGINT, &sig_handler);

    while (inString = readline("# "))
    {
        char *strDup = strdup(inString);

        parsedcmd = parseString(inString);

        int numArgs = numTokens(strDup);
        free(strDup);

        //parse for redirects, pipes
        cpid = fork();
        if (cpid == 0)
        {
            //check for jobs first

            //check for pipes first
            int pipeInd = indOfSym(parsedcmd, "|");
            if (pipeInd > -1)
            {
                char **leftSub = (char **)malloc(500 * sizeof(char *));
                char **rightSub = (char **)malloc(500 * sizeof(char *));
                for (int j = 0; j < pipeInd; j++)
                {
                    leftSub[j] = parsedcmd[j];
                }
                int r_ind = 0;
                for (int j = pipeInd + 1; j < numArgs; j++)
                {
                    rightSub[r_ind] = parsedcmd[j];
                    r_ind++;
                }
                int pfd[2];
                pipe(pfd);
                pid_t p1 = fork();
                if (p1 == 0)
                {
                    close(pfd[0]);
                    dup2(pfd[1], STDOUT_FILENO);
                    execvp(leftSub[0], leftSub);
                }
                pid_t p2 = fork();
                if (p2 == 0)
                {
                    close(pfd[1]);
                    dup2(pfd[0], STDIN_FILENO);
                    execvp(rightSub[0], rightSub);
                }
                close(pfd[0]);
                close(pfd[1]);
                // waitpid(p1, NULL, 0);
                // waitpid(p2, NULL, 0);
                wait((int *)NULL);
                wait((int *)NULL);
                continue;
            }
            int inputInd = indOfSym(parsedcmd, "<");
            int outputInd = indOfSym(parsedcmd, ">");
            int errInd = indOfSym(parsedcmd, "2>");
            int lastArgInd = numArgs - 1;
            if (inputInd > -1)
            {
                char *file = parsedcmd[inputInd + 1];
                int fd = open(file, O_RDONLY);
                dup2(fd, 0);
                lastArgInd = min(lastArgInd, inputInd - 1);
            }
            if (outputInd > -1)
            {
                char *file = parsedcmd[outputInd + 1];
                int fd = open(file, O_WRONLY);
                dup2(fd, 1);
                lastArgInd = min(lastArgInd, outputInd - 1);
            }
            if (errInd > -1)
            {
                char *file = parsedcmd[errInd + 1];
                int fd = open(file, O_WRONLY);
                dup2(fd, 2);
                lastArgInd = min(lastArgInd, errInd - 1);
            }
            char **argSub = (char **)malloc(500 * sizeof(char *));
            for (int i = 0; i <= lastArgInd; i++)
            {
                argSub[i] = parsedcmd[i];
            }
            execvp(argSub[0], argSub);
            // if (containsOutput(parsedcmd))
            // {
            //     execOutRedirect(numArgs, parsedcmd);
            // }
            // if (containsInput(parsedcmd))
            // {
            //     execInRedirect(numArgs, parsedcmd);
            // }
            //execvp(parsedcmd[0], parsedcmd);
        }
        else
        {
            wait((int *)NULL);
        }
        //execvp(parsedcmd[0], parsedcmd);
    }
}