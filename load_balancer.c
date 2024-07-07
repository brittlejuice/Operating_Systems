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

int create_message_queue(char *path, int project_id)
{
    key_t key;
    int msgqid;

    if ((key = ftok(path, project_id)) == -1)
    {
        perror("ftok");
        exit(-1);
    }

    if ((msgqid = msgget(key, PERMS | IPC_CREAT)) == -1)
    {
        perror("msgget");
        exit(-1);
    }

    return msgqid;
}

Request recieve_request(int msgqid)
{
    Request request;
    if (msgrcv(msgqid, &request, REQUEST_SIZE, 1, 0) == -1)
    {
        perror("msgrcv");
        exit(-1);
    }

    if (request.op == -1 )
        printf("Recieved a message form the cleanup process with PID: %d: %s\n", request.snum, request.mtext);


    else
        printf("Recieved a request from client with Client ID: %d\n", request.snum);

    return request;
}

void forward_request(int msgqid, int msgtype, int seqnum, int opnum, char graphfile[])
{
    Request request;
    request.mtype = msgtype;
    request.snum = seqnum;
    request.op = opnum;
    strcpy(request.mtext, graphfile);

    if (msgsnd(msgqid, &request, REQUEST_SIZE, 0) == -1)
    {
        perror("msgsnd");
        exit(-1);
    }
}

void send_reply(int msgqid, int seqnum, char msg[])
{
    Reply reply;
    reply.mtype = seqnum + 5;
    strcpy(reply.mtext, msg);

    if (msgsnd(msgqid, &reply, REPLY_SIZE, 0) == -1)
    {
        perror("msgsnd");
        exit(-1);
    }
}

void terminate_balancer(int msgqid)
{
    send_reply(msgqid, 0, "Termination REquest Accepted");

    printf("TERMINATION LOG:\n\n");
    printf("Beggining Cleanup Process...\n");

    forward_request(msgqid, 2, getpid(), -1, "Requesting termination of the server.");
    forward_request(msgqid, 3, getpid(), -1, "Requesting termination of the server.");
    forward_request(msgqid, 4, getpid(), -1, "Requesting termination of the server.");

    printf("Channel 1 is closed no further requests will be accepted.\n");
    sleep(5);

    printf("Deleting the message queue all ongoing requests are processed.\n");
    if (msgctl(msgqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl");
        exit(- 1);
    }

    printf("Message queue successfully deleted.\n");
}

int main()
{
    int msgqid = create_message_queue("load_balancer.c", 124);

    int end=0;
    while (!end)
    {
        Request request = recieve_request(msgqid);

        int msgtyp;
        if (request.op == -1)
            end=1;

        else if (request.op == 1 || request.op == 2)
            msgtyp = 2;
        
        else if (request.snum % 2 != 0)
            msgtyp = 3;
        
        else
            msgtyp = 4;
          
        forward_request(msgqid, msgtyp, request.snum, request.op, request.mtext);
    }

    terminate_balancer(msgqid);
    printf("Terminating Load Balancer.....\n");
    exit(0);
}