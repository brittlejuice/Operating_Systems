#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#define PERMS 0777
#define MAX_MSG_SIZE 100*sizeof(char)
#define REQUEST_SIZE (2*sizeof(int)+MAX_MSG_SIZE)
#define REPLY_SIZE (MAX_MSG_SIZE)

typedef struct request
{
    long mtype;
    int snum, op;
    char mtext[100];
}Request;

typedef struct reply
{
    long mtype;
    char mtext[100];
}Reply;

int get_message_queue(char *path, int project_id)
{
    key_t key;
    int msgqid;

    if ((key = ftok(path, project_id)) == -1)
    {
        perror("ftok");
        exit(-1);
    }

    if ((msgqid = msgget(key, PERMS)) == -1)
    {
        perror("msgget");
        exit(-1);
    }

    return msgqid;
}

int get_shared_memory(char *path, int project_id)
{
    key_t key;
    int shmid;

    if ((key = ftok(path, project_id)) == -1)
    {
        perror("ftok");
        exit(-1);
    }

    if ((shmid = shmget(key, 1024, PERMS)) == -1)
    {
        perror("shmid");
        exit(-1);
    }

    return shmid;
}

void send_termination_request(int msgqid)
{
    Request request;
    request.mtype = 1;
    request.snum = getpid();
    request.op = -1;

    strcpy(request.mtext, "Requesting termination of the Load Balancer and Primary and Secondary Servers.");

    if (msgsnd(msgqid, &request, REQUEST_SIZE, 0) == -1)
    {
        perror("msgsnd");
        exit(-1);
    }

    Reply reply;
    if (msgrcv(msgqid, &reply, REPLY_SIZE, 5, 0) == -1)
    {
        perror("msgrcv");
        exit(-2);
    }

    printf("\nRecieved a message from the Load Balancer: %s\n", reply.mtext);
}


int main()
{
    int msgqid = get_message_queue("load_balancer.c", 124);

    int terminate = 0;
    while (!terminate)
    {
        char ch;
        printf("Do you want the server to terminate? Press Y for Yes and N for No: \n"); 
        scanf("\n%c", &ch);
        
        if (ch == 'Y')
        {
            send_termination_request(msgqid);
            printf("Server  terminating.....\n");
            terminate=1;
        }
    }

    printf("Cleanup Process Exiting.....\n");
    exit(0);
}