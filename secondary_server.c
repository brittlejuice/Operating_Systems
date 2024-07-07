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

typedef struct request
{
    long mtype;
    int snum, op;
    char mtext[100];
}Request;

typedef struct sychronizer
{
    int readercount;
    int hash[100];
}Synchronizer;

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

typedef struct queue
{
    int first, last;
    int arr[100][2];
}Queue;

typedef struct grp_args
{
    int node;
    int p;
    Graph graph;
    Queue* queue;
    sem_t* mutex;
    char* res;
}graph_args;

Queue* create_queue()
{
    Queue* q  = (Queue*)malloc(sizeof(Queue));
    q->last=-1;
    q->first=0;
    for (int i=0; i<100; i++)
    {
        q->arr[i][0]=0;
        q->arr[i][1]=0;
    }
        
    return q;
}

void push(Queue* q, int node, int parent)
{
    q->last++;
    q->arr[q->last][0]=node;
    q->arr[q->last][1]=parent;
}

int* front(Queue* q)
{
    if (q->last >= q->first)
        return q->arr[q->first];
    
    else
        printf("Error.\n");
}

void pop(Queue* q)
{
    if (q->last >= q->first)
        q->first++;
    
    else
        printf("Error.\n");
}

int size(Queue* q)
{
    return q->last - q->first + 1;
}

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

Request recieve_request(int msgqid, int server_num)
{
    Request request;
    if (msgrcv(msgqid, &request, REQUEST_SIZE, server_num+2, 0) == -1)
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

void delete_shared_memory(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
        exit(-1);
    }

    printf("\nShared Memory Segment Deleted.\n");
}

int min_writer_snum(Synchronizer* syn)
{
    int min=1000;
    for (int i=1; i<100; i++)
    {
        if (syn->hash[i])
            return i;
    }

    return min;
}

void* dfs(void *param)
{
    graph_args* args = (graph_args*)param;

    int child=0;
    int node = args->node;
    int parent = args->p;
    int n = args->graph.n;
    int adjmat[n][n];
    for (int i=0; i<n; i++)
        for (int j=0; j<n; j++)
            adjmat[i][j] = args->graph.adjmat[i][j];
    
    pthread_t child_tid[n];
    graph_args child_args[n];

    for (int i=0; i<n; i++)
    {
        if (adjmat[node][i] && i!=parent)
        {
            child_args[child].node = i;
            child_args[child].p = node;
            child_args[child].res = args->res;

            child_args[child].graph.n = n;
            for (int p=0; p<n; p++)
                for (int q=0; q<n; q++)
                    child_args[child].graph.adjmat[p][q] = adjmat[p][q];
            
            pthread_create(&child_tid[child], NULL, dfs, (void*)&child_args[child]);
            child++;
        }
    }

    if (child==0)
    {
        char num[5];
        sprintf(num, "%d ", node+1);
        strcat(args->res, num);
    }
    
    else
    {
        for (int i=0; i<child; i++)
            pthread_join(child_tid[i], NULL);
    }

    pthread_exit(NULL);
}
                
void* bfs(void* param)
{
    graph_args* args = (graph_args*)param;

    int node = args->node;
    int parent = args->p;
    int n = args->graph.n;
    int adjmat[n][n];
    for (int i=0; i<n; i++)
        for (int j=0; j<n; j++)
            adjmat[i][j] = args->graph.adjmat[i][j];

    for (int i=0; i<n; i++)
    {
        if (adjmat[node][i] && i!=parent)
        {
            sem_wait(args->mutex);
            push(args->queue, i, node);
            sem_post(args->mutex);
        }
            
    }
    
    pthread_exit(0);
}

void* thread_routine(void* args)
{
    thread_args *arguments = (thread_args*)args;

    char semname1[100], semname2[100];
    strcat(semname1, arguments->req->mtext);
    strcat(semname1, "writer");
    strcat(semname2, arguments->req->mtext);
    strcat(semname2, "mutex");

    printf("%s %s\n", semname1, semname2);

    sem_t* write, *mutex;
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
    
    int shmid = get_shared_memory("client.c", arguments->req->snum);
    int synid = get_synchronizer(arguments->req->mtext, 169);

    int* start = (int*)shmat(shmid, NULL, 0);
    
    graph_args g_args;
    g_args.node = (*start)-1;
    g_args.p = -1;

    char result[100];
    g_args.res = result;

    Synchronizer* syn = (Synchronizer*)shmat(synid, NULL, 0);

    int val;
    sem_getvalue(write, &val);
    printf("%d\n", val);
    sem_wait(mutex);
    syn->readercount++;
    printf("%d\n", syn->readercount);
    printf("%d\n", min_writer_snum(syn));
    if (syn->readercount == 1 || min_writer_snum(syn) > arguments->req->snum)
        sem_wait(write);
    
    sem_post(mutex);

    // Reader
    FILE* fd;
    if ((fd = fopen(arguments->req->mtext, "r")) == NULL)
    {
        perror("fopen");
        exit(-1);
    }

    fscanf(fd, "%d", &g_args.graph.n);

    for (int i=0; i<g_args.graph.n; i++)
        for (int j=0; j<g_args.graph.n; j++)
            fscanf(fd, "%d", &g_args.graph.adjmat[i][j]);

    sem_wait(mutex);
        syn->readercount--;

    if (syn->readercount == 0 || min_writer_snum(syn) < arguments->req->snum)
        sem_post(write);
    
    sem_post(mutex);
    
    if (arguments->req->op == 3)
    {
        pthread_t tid;
        pthread_create(&tid, NULL, dfs, (void*)&g_args);
        pthread_join(tid, NULL);
    }
    
    else
    {
        sem_t mutex;
        Queue* queue = create_queue();
        sem_init(&mutex, 0, 1);

        push(queue, g_args.node, -1);

        while (size(queue) != 0)
        {
            int child = size(queue);
            //printf("Size: %d\n", child);
            pthread_t child_tid[child];
            graph_args child_args[child];

            for (int i=0; i<child; i++)
            {
                sem_wait(&mutex);
                int* arr = front(queue);
                pop(queue);
                // printf("Pop: %d %d\n", arr[0], arr[1]); 
                sem_post(&mutex);

                char num[5];
                sprintf(num, "%d ", arr[0]+1);
                strcat(result, num);

                child_args[i].node = arr[0];
                child_args[i].p = arr[1];
                child_args[i].queue = queue;
                child_args[i].mutex = &mutex;
                child_args[i].graph.n = g_args.graph.n;
                
                for (int p=0; p<g_args.graph.n; p++)
                    for (int q=0; q<g_args.graph.n; q++)
                        child_args[i].graph.adjmat[p][q] = g_args.graph.adjmat[p][q];
                    
                pthread_create(&child_tid[i], NULL, bfs, (void*)&child_args[i]);
            }

            for (int i=0; i<child; i++)
                pthread_join(child_tid[i], NULL);
        }

        sem_destroy(&mutex);
    }

    shmdt(start);
    shmdt(syn);
    send_reply(arguments->mqid, arguments->req->snum, result);
    printf("Successfully completed a Request with Sequence Number: %d\n", arguments->req->snum);
    sem_unlink(semname1);
    sem_unlink(semname2);
    //delete_shared_memory(synid);
    pthread_exit(0);
}

void terminate_server(int msgqid, pthread_t tid[], int servernum, int reqnum)
{
    printf("Waiting for all pending requests to finish completion......\n");

    for (int i=0; i<reqnum; i++)
        pthread_join(tid[i], NULL);
    
    printf("All pending requests completed server can terminate.\n");
}

int main(int argc, char* argv[])
{
    int server_num = atoi(argv[1]);

    int msgqid = get_message_queue("load_balancer.c", 124);
    pthread_t tid[100];
    int reqnum = 0;

    int end=0;
    while (!end)
    {
        Request request = recieve_request(msgqid, server_num);

        if (request.op == -1)
            end=1;
        
        else
        {
            thread_args args;
            args.mqid = msgqid;
            args.req = &request;

            pthread_create(&tid[reqnum], NULL, thread_routine, (void*)&args);
            reqnum++;
        }
    }

    terminate_server(msgqid, tid, server_num, reqnum);
    printf("Terminating Server.....\n");
    exit(0);
}
