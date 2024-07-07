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
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h> 
#define PERMS 0777
#define MAX_MSG_SIZE 100*sizeof(char)
#define REQUEST_SIZE (2*sizeof(int)+MAX_MSG_SIZE)
#define REPLY_SIZE (MAX_MSG_SIZE)

typedef struct graph
{
    int n;
    int adjmat[100][100];
}Graph;

typedef struct sychronizer
{
    int readercount;
    int hash[100];
}Synchronizer;

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

typedef struct thread_arg
{
    int mqid;
    Request* req;
}thread_args;

int get_message_queue(char *path, int project_id)
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
    if (msgrcv(msgqid, &request, REQUEST_SIZE, 2, 0) == -1)
    {
        perror("msgrcv");
        exit(-1);
    }

    if (request.op == -1 )
        printf("Recieved a message form the Load Balancer with PID: %d: %s\n", request.snum, request.mtext);

    else
        printf("Recieved a request with Sequence Number: %d\n", request.snum);

    return request;
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

int create_synchronizer(char* filename, int proj_id)
{
    key_t key;
    int shmid;

    if ((key = ftok(filename, proj_id)) == -1)
    {
        perror("ftok");
        exit(-1);
    }

    if ((shmid = shmget(key, 1024, PERMS | IPC_CREAT)) == -1)
    {
        perror("shmid");
        exit(-1);
    } 

    Synchronizer* syn = (Synchronizer*)shmat(shmid, NULL, 0);

    syn->readercount=0;
    for (int i=0; i<100; i++)
        syn->hash[i]=0;

    shmdt(syn);
    return shmid;
}

void delete_shared_memory(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
        exit(-1);
    }

    printf("\nShared Memory Segment Deleted.\n");
}

int get_synchronizer(char* filename, int proj_id)
{
    key_t key;
    int shmid;

    if ((key = ftok(filename, proj_id)) == -1)
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

void* create_update_graph(void* args)
{
    thread_args *arguments = (thread_args*)args;

    int shmid = get_shared_memory("client.c", arguments->req->snum);
    Graph* graph = (Graph*)shmat(shmid, NULL, 0);

    int synid;
    sem_t *write, *mutex;

    char semname1[100], semname2[100];
    strcat(semname1, arguments->req->mtext);
    strcat(semname1, "writer");
    strcat(semname2, arguments->req->mtext);
    strcat(semname2, "mutex");

    if ((write = sem_open(semname1, O_CREAT, PERMS, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(-1);
    }

    if ((mutex = sem_open(semname2, O_CREAT, PERMS, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(-1);
    }
    
    FILE* fd;
    if ((fd = fopen(arguments->req->mtext, "w")) == NULL)
    {
        perror("fopen");
        exit(-1);
    }

    if (arguments->req->op == 1)
        synid = create_synchronizer(arguments->req->mtext, 169);
    
    else
        synid = get_synchronizer(arguments->req->mtext, 169);

    Synchronizer* syn = (Synchronizer*)shmat(synid, NULL, 0);

    syn->hash[arguments->req->snum]=1;
    sem_wait(write);
    int val;
    sem_getvalue(write, &val);
    printf("%d\n", val);
    fprintf(fd, "%d\n", graph->n);
    for (int i=0; i<graph->n; i++)
    {
        for (int j=0; j<graph->n; j++)
            fprintf(fd, "%d ", graph->adjmat[i][j]);
        
        fprintf(fd, "\n");
    }
    syn->hash[arguments->req->snum]=0;
    sem_post(write);

    fclose(fd);
    shmdt(graph);
    shmdt(syn);

    if (arguments->req->op == 1)
        send_reply(arguments->mqid, arguments->req->snum, "File successfully added.");
    
    else
        send_reply(arguments->mqid, arguments->req->snum, "File successfully updated.");

    printf("\nSuccessfully completed a Request with Sequence Number: %d\n", arguments->req->snum);

    sem_unlink(semname1);
    sem_unlink(semname2);
    //delete_shared_memory(synid);
    pthread_exit(NULL);
}

void terminate_server(int msgqid, pthread_t tid[], int reqnum)
{
    printf("Waiting for all pending requests to finish completion......\n");

    for (int i=0; i<reqnum; i++)
        pthread_join(tid[i], NULL);
    
    printf("All pending requests completed server can terminate.\n");
}

int main()
{
    int msgqid = get_message_queue("load_balancer.c", 124);
    pthread_t tid[100];
    int reqnum = 0;

    int end=0;
    while (!end)
    {
        Request request = recieve_request(msgqid);

        if (request.op == -1)
            end=1;
        
        else
        {
            thread_args args;
            args.mqid = msgqid;
            args.req = &request;

            pthread_create(&tid[reqnum], NULL, create_update_graph, (void*)&args);
            reqnum++;
        }
    }

    terminate_server(msgqid, tid, reqnum);
    printf("Terminating Server.....\n");
    exit(0);
}
