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
#include <errno.h>
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

Job *head = NULL;
Job *tail = NULL;

Job *doneListTail = NULL;

void clearDoneList()
{
    while (doneListTail != NULL)
    {
        Job *prev = doneListTail->prev;
        free(doneListTail);
        doneListTail = prev;
    }
}

void addToDoneList(pid_t job_id, pid_t job_pgid, char *cmdstr, char *procStatus, int jobNum)
{
    Job *job = (Job *)malloc(sizeof(Job));
    job->job_pid = job_id;
    job->pgid = job_pgid;
    job->jobstring = cmdstr;
    job->status = procStatus;
    job->jobNum = jobNum;
    job->next = NULL;
    job->prev = doneListTail;
    if (doneListTail == NULL)
    {
        doneListTail = job;
    }
    else
    {
        doneListTail->next = job;
        doneListTail = job;
    }
}

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
    if (indOfPid == NULL)
    {
        return;
    }
    Job toRemove = *indOfPid;
    if (tail == head)
    {
        head = NULL;
        tail = NULL;
    }
    else if (indOfPid == head)
    {
        toRemove.next->prev = NULL;
        head = toRemove.next;
    }
    else if (indOfPid == tail)
    {
        toRemove.prev->next = NULL;
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
    Job *doneIt = doneListTail;
    bool mostRecentIsDone = false;
    char *amp = " &";
    while (doneIt != NULL)
    {
        char *strWithAmp = strdup(doneIt->jobstring);
        strcat(strWithAmp, amp);
        if (tail == NULL && !mostRecentIsDone || tail != NULL && doneListTail->jobNum > tail->jobNum) // no jobs in job list OR most recent is job is done, SO print with +
        {
            printf("[%d]+\t%s\t\t\t%s\n", doneListTail->jobNum, doneListTail->status, strWithAmp);
            mostRecentIsDone = true;
        }
        else
        {
            printf("[%d]-\t%s\t\t\t%s\n", doneIt->jobNum, doneIt->status, strWithAmp);
        }
        doneIt = doneIt->prev;
    }
    clearDoneList();
    if (head == NULL)
    {
        return;
    }
    Job *it = head;
    while (it != NULL)
    {
        char *strWithAmp = strdup(it->jobstring);

        strcat(strWithAmp, amp);

        if (!mostRecentIsDone && it == tail)
        {
            if (strcmp(it->status, "Running") == 0)
            {
                printf("[%d]+\t%s\t\t\t%s\n", it->jobNum, it->status, strWithAmp);
            }
            else
            {
                printf("[%d]+\t%s\t\t\t%s\n", it->jobNum, it->status, it->jobstring);
            }
            break;
        }
        if (strcmp(it->status, "Running") == 0)
        {
            printf("[%d]-\t%s\t\t\t%s\n", it->jobNum, it->status, strWithAmp);
        }
        else
        {
            printf("[%d]-\t%s\t\t\t%s\n", it->jobNum, it->status, it->jobstring);
        }
        it = it->next;
    }
}

Job *findLastStopped()
{
    if (head == NULL)
    {
        return NULL;
    }
    Job *it = tail;
    char *str = "Stopped";
    while (it != NULL)
    {
        if (strcmp(it->status, str) == 0)
        {
            return it;
        }
        it = it->prev;
    }
    return NULL;
}

void backgroundJob()
{
    if (findLastStopped() == NULL)
    {
        return;
    }
    Job *lastStopped = findLastStopped();
    kill(-1 * lastStopped->pgid, SIGCONT);
    lastStopped->status = "Running";
    char *plusOrMin = "+";
    if (lastStopped->jobNum < tail->jobNum)
    {
        plusOrMin = "-";
    }
    char *amp = " &"; // might've already been a background job, need to check if string alr has the &
    char *dispStr = strdup(lastStopped->jobstring);
    strcat(dispStr, amp);
    printf("[%d]%s\t%s\n", lastStopped->jobNum, plusOrMin, dispStr);
}

void foregroundJob(pid_t shellpid)
{
    if (tail == NULL)
    {
        return;
    }
    int last_pgid = tail->pgid;
    char *jobStr = tail->jobstring;
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    tcsetpgrp(0, last_pgid);
    kill(-1 * last_pgid, SIGCONT);
    printf("%s\n", jobStr);
    removeFromJobList(tail->job_pid);
    int status;
    pid_t wait_pid;
    do
    {
        wait_pid = waitpid(-1 * last_pgid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
    bool stopped = WIFSTOPPED(status);
    if (stopped)
    {
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        tcsetpgrp(0, getpgid(shellpid));
        addToJobList(last_pgid, last_pgid, jobStr, "Stopped");
    }
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    tcsetpgrp(0, getpgid(shellpid));
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
    Job *it = head;
    int status;
    pid_t wait_pid;
    char *done = "Done";
    while (it != NULL)
    {
        if (wait_pid = waitpid(it->job_pid, &status, WNOHANG) > 0) // change back to while loop? change pgid to just it->job_pid
        {
            if (WIFEXITED(status))
            {
                it->status = done;
                addToDoneList(it->job_pid, it->pgid, it->jobstring, it->status, it->jobNum);
                removeFromJobList(it->job_pid);
            }
        }
        it = it->next;
    }
}

void execLine(char **parsedcmd, int lastArgInd)
{

    int inputInd = indOfSym(parsedcmd, "<");
    int outputInd = indOfSym(parsedcmd, ">");
    int errInd = indOfSym(parsedcmd, "2>");

    if (inputInd > -1)
    {
        char *file = parsedcmd[inputInd + 1];
        int fd = open(file, O_RDONLY); // open returns -1 on error, use to fix hanging issue with wc example from FAQ
        if (fd == -1)
        {
            exit(1);
        }
        dup2(fd, 0);
        lastArgInd = min(lastArgInd, inputInd - 1);
    }
    if (outputInd > -1)
    {
        char *file = parsedcmd[outputInd + 1];
        int fd = open(file, O_RDWR | O_CREAT, 0777);
        dup2(fd, 1);
        lastArgInd = min(lastArgInd, outputInd - 1);
    }
    if (errInd > -1)
    {
        char *file = parsedcmd[errInd + 1];
        int fd = open(file, O_RDWR | O_CREAT, 0777);
        dup2(fd, 2);
        lastArgInd = min(lastArgInd, errInd - 1);
    }
    char **argSub = (char **)malloc(500 * sizeof(char *));
    for (int i = 0; i <= lastArgInd; i++)
    {
        argSub[i] = parsedcmd[i];
    }
    execvp(argSub[0], argSub);
    return;
}

void pipeExec(char *jobStr, int numArgs, bool bg, pid_t shellpid)
{
    char *pipeStr = strdup(jobStr);
    char **parsedcmd = parseString(jobStr);
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
        setpgid(0, 0);
        tcsetpgrp(0, getpid());

        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        execLine(leftSub, pipeInd - 1);
    }
    setpgid(p1, p1);
    tcsetpgrp(0, p1);
    pid_t p2 = fork();
    if (p2 == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        setpgid(0, p1);

        close(pfd[1]);
        dup2(pfd[0], STDIN_FILENO);
        execLine(rightSub, numArgs - 1 - pipeInd);
    }
    setpgid(p2, p1);
    close(pfd[0]);
    close(pfd[1]);
    if (!bg)
    {
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        int status;
        waitpid(-1, &status, WUNTRACED);
        waitpid(-1, &status, WUNTRACED);
        if (WIFSTOPPED(status))
        {
            addToJobList(p2, getpgid(p2), pipeStr, "Stopped");
            tcsetpgrp(0, getpgid(shellpid));
        }
    }
    else
    {
        tcsetpgrp(0, getpgid(shellpid));
        waitpid(-1, NULL, WNOHANG);
        waitpid(-1, NULL, WNOHANG);
    }
}

char *trimWhitespace(char *str)
{
    char *last;
    while (*str == ' ')
    {
        str++;
    }
    if (*str == 0)
    {
        return str;
    }
    last = str + strlen(str) - 1;
    while (last > str && *last == ' ')
    {
        last--;
    }
    last[1] = '\0';
    return str;
}

int main(int argc, char **argv)
{

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
        inString = trimWhitespace(inString);
        char *strDup = strdup(inString);

        parsedcmd = parseString(inString);

        char *jobStr = strdup(strDup);
        int numArgs = numTokens(strDup);

        free(strDup);

        bool runInBG = false;
        char *ifAmp;

        if (numArgs > 1 && strcmp(parsedcmd[numArgs - 1], "&") == 0)
        {
            parsedcmd[numArgs - 1] = NULL;
            runInBG = true;
            ifAmp = strdup(jobStr);
            int size = strlen(ifAmp);
            ifAmp[size - 1] = ' '; // doesnt get rid of space before/after & -- could be problem
            ifAmp[size - 2] = ' ';
            ifAmp = trimWhitespace(ifAmp);
        }

        if (numArgs == 0)
        {
            continue;
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "bg") == 0)
        {
            backgroundJob();
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "fg") == 0)
        {
            foregroundJob(shellpid);
        }
        else if (numArgs == 1 && indOfSym(parsedcmd, "jobs") == 0)
        {
            displayJobs();
        }
        else if (indOfSym(parsedcmd, "|") > -1 && !runInBG)
        {
            pipeExec(jobStr, numArgs, runInBG, shellpid);
        }

        // execute command in background
        else if (runInBG)
        {
            pid_t ampfork = fork();
            if (ampfork == 0)
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                setpgid(0, 0);
                pid_t currPid = getpid();
                if (indOfSym(parsedcmd, "|") > -1)
                {
                    pipeExec(jobStr, numArgs - 1, runInBG, shellpid);
                    continue;
                }
                else
                {
                    execLine(parsedcmd, numArgs - 2);
                    continue;
                }
            }
            else
            {
                setpgid(ampfork, ampfork);
                int status;
                addToJobList(ampfork, ampfork, ifAmp, "Running");
                pid_t pid = waitpid(-1 * ampfork, &status, WNOHANG);
            }
        }
        else
        {
            pid_t cmdfork = fork();

            if (cmdfork == 0)
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);

                setpgid(0, 0);

                tcsetpgrp(0, getpid());

                execLine(parsedcmd, numArgs - 1);

                exit(1); // exits if invalid command
            }
            else
            {
                setpgid(cmdfork, cmdfork);
                tcsetpgrp(0, cmdfork);

                int status;
                pid_t wait_pid = waitpid(-1 * cmdfork, &status, WUNTRACED);
                bool stopped = WIFSTOPPED(status); // tells us if process was stopped with ^Z (returns true)
                if (stopped)
                {
                    // add to jobs list
                    addToJobList(cmdfork, cmdfork, jobStr, "Stopped");
                    tcsetpgrp(0, getpgid(shellpid));
                }
            }
        }
        tcsetpgrp(0, getpgid(shellpid));
    }
}