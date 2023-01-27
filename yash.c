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

typedef enum
{
    false,
    true
} bool;

typedef struct Job
{
    pid_t job_pid; // right child pid if pipe -- consider separate left/right child pid entries if necessary
    pid_t pgid;
    int jobNum;
    char *status;
    char *jobstring; // maybe parsedcmd array instead ??
    struct Job *next;
    struct Job *prev;
} Job;

// Job **jobList;
Job *head = NULL;
Job *tail = NULL;

Job *findJobByPID(pid_t pid)
{
    Job *ptr = head;
    while (ptr != NULL)
    {
        if (ptr->job_pid == pid)
        {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void addToJobList(pid_t job_id, pid_t job_pgid, char *cmdstr, char *procStatus)
{
    Job *job = (Job *)malloc(sizeof(Job));
    job->job_pid = job_id;
    job->pgid = job_pgid;
    job->jobstring = cmdstr;
    job->status = procStatus;

    if (head == NULL)
    {
        head = job;
        tail = job;
        job->jobNum = 1;
        job->prev = NULL;
        job->next = NULL;
    }
    else
    {
        job->jobNum = 1 + (tail->jobNum);
        job->next = NULL;
        job->prev = tail;
        tail->next = job;
        tail = job;
    }
}

void removeFromJobList(pid_t pid)
{
    Job *indOfPid = findJobByPID(pid);
    Job toRemove = *indOfPid;
    if (indOfPid == head)
    {
        head = NULL;
        tail = NULL;
    }
    else if (indOfPid == tail)
    {
        tail = toRemove.prev;
    }
    else
    {
        Job *prevJob = toRemove.prev;
        prevJob->next = toRemove.next;
        toRemove.next->prev = prevJob;
    }
    free(indOfPid);
}

void displayJobs()
{
    if (head == NULL)
    {
        return;
    }
    Job *it = head;
    while (it->next != NULL)
    {
        printf("[%d]-\t%s\t\t\t%s\n", it->jobNum, it->status, it->jobstring);
        it = it->next;
    }
    printf("[%d]+\t%s\t\t\t%s\n", it->jobNum, it->status, it->jobstring);
}

void backgroundJob()
{
}

void foregroundJob()
{
    kill(-1 * tail->pgid, SIGCONT);
    tcsetpgrp(0, tail->pgid);
    printf("%s\n", tail->jobstring);
    removeFromJobList(tail->job_pid);
}

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
    char **arrOfChars = (char **)malloc((1000) * sizeof(char *));
    char *token = (char *)malloc(30 * sizeof(char));
    int ind = 0;
    while (token = strtok_r(str, " ", &str))
    {
        arrOfChars[ind] = token;
        ind++;
    }
    return arrOfChars;
}

void sigchild_handler(int signo)
{
    int status;
    // removeFromJobList();
}

void execLine(char **parsedcmd, int lastArgInd)
{
    // setpgid(cpid, cpid);  do this here or in main? pro: can access cpid, con(?): need to do in pipe as well
    // tcsetpgrp(0, getpgrp() ?) set terminal control to process group that is abt to be executed
    // tcsetpgrp(0, getpgrp());
    // signal(SIGINT, SIG_DFL); //any process/job that is executed must call execLine, so we set signal in the method here
    // signal(SIGTSTP, SIG_DFL);

    int inputInd = indOfSym(parsedcmd, "<");
    int outputInd = indOfSym(parsedcmd, ">");
    int errInd = indOfSym(parsedcmd, "2>");

    if (inputInd > -1)
    {
        char *file = parsedcmd[inputInd + 1];
        int fd = open(file, O_RDONLY); // open returns -1 on error, use to fix hanging issue with wc example from FAQ
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
}

void pipeExec(char **parsedcmd, int numArgs)
{
    int pipeInd = indOfSym(parsedcmd, "|");

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
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        execLine(leftSub, pipeInd - 1);
    }
    // setpgid(p1, 0);
    pid_t p2 = fork();
    if (p2 == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        // setpgid(0, p1);
        close(pfd[1]);
        dup2(pfd[0], STDIN_FILENO);
        execLine(rightSub, numArgs - 1 - pipeInd);
    }
    // tcsetpgrp(0, p1);
    close(pfd[0]);
    close(pfd[1]);
    wait((int *)NULL);
    wait((int *)NULL);
}

int main(int argc, char **argv)
{
    head = NULL;
    tail = NULL;

    pid_t shellpid = getpid();

    char *inString;
    char **parsedcmd;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    signal(SIGCHLD, &sigchild_handler);

    while (inString = readline("# "))
    {
        char *strDup = strdup(inString);

        parsedcmd = parseString(inString);

        char *jobStr = strdup(strDup);
        int numArgs = numTokens(strDup);

        free(strDup);

        if (numArgs == 0)
        {
            continue;
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "bg") == 0)
        {
            // bg
            backgroundJob();
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "fg") == 0)
        {
            // fg
            foregroundJob();
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "jobs") == 0)
        {
            displayJobs();
            // jobs
            // display list of jobs. We have head ptr to jobs linked list, write printJobs method & output to console
        }
        else if (indOfSym(parsedcmd, "|") > -1)
        {
            pipeExec(parsedcmd, numArgs);
        }

        // execute command normally
        else
        {
            pid_t cmdfork = fork();

            if (cmdfork == 0)
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);

                // pid_t pgid = getpid();
                // setpgid(getpid(), pgid);
                // tcsetpgrp(0, pgid);

                setpgid(0, 0);
                tcsetpgrp(0, getpid());

                execLine(parsedcmd, numArgs - 1);
                exit(1); // exits if invalid command
            }
            else
            {
                // pid_t pgid = cmdfork;
                // setpgid(cmdfork, pgid);
                // tcsetpgrp(0, pgid);
                setpgid(cmdfork, cmdfork);
                tcsetpgrp(0, cmdfork);

                int status;
                pid_t wait_pid = waitpid(-1 * cmdfork, &status, WUNTRACED);
                bool stopped = WIFSTOPPED(status); // tells us if process was stopped (true) or terminated (false)
                if (stopped)
                {
                    // add to jobs list
                    addToJobList(cmdfork, cmdfork, jobStr, "Stopped"); // might have to strdup inString
                    tcsetpgrp(0, getpgid(shellpid));
                }
                else
                {
                    tcsetpgrp(0, getpgid(shellpid));
                }
            }
        }
        tcsetpgrp(0, getpgid(shellpid));
    }
}