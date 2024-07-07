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

int create_shared_memory(char *path, int project_id)
{
    key_t key;
    int shmid;

    if ((key = ftok(path, project_id)) == -1)
    {
        perror("ftok");
        exit(-1);
    }

    if ((shmid = shmget(key, 1024, PERMS | IPC_CREAT)) == -1)
    {
        perror("shmid");
        exit(-1);
    }
    
    printf("\nCreated Shared Memory Segment.\n");

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

void send_request(int msgqid, int seqnum, int opnum, char graphfile[])
{
    Request request;
    request.mtype = 1;
    request.snum = seqnum;
    request.op = opnum;
    strcpy(request.mtext, graphfile);

    if (msgsnd(msgqid, &request, REQUEST_SIZE, 0) == -1)
    {
        perror("msgsnd");
        exit(-1);
    }
}

Reply recieve_reply(int msgqid, int seqnum)
{
    Reply reply;
    if (msgrcv(msgqid, &reply, REPLY_SIZE, seqnum + 5, 0) == -1)
    {
        perror("msgrcv");
        exit(-1);
    }

    return reply;
}

int main()
{
    int msgqid = get_message_queue("load_balancer.c", 124);

    int end=0;
    while (!end)
    {
        printf("\nCLIENT MENU:\n\n1. Add a new graph to the database.\n2. Modify an existing graph of the database.\n3. Perform DFS on an existing graph of the database.\n4. Perform BFS on an existing graph of the database.\n5. Exit.\n\n");

        int seqnum;
        int opnum;
        char graphfile[100];

        printf("Enter Sequence Number:\n");
        scanf("%d", &seqnum);

        printf("\nEnter Operation Number:\n");
        scanf("%d", &opnum);

        printf("\nEnter Graph File Name:\n");
        scanf("%s", graphfile);

        int shmid = create_shared_memory("client.c", seqnum);

        if (opnum == 1 || opnum == 2)
        {
            Graph* graph = (Graph*)shmat(shmid, NULL, 0);

            int num;
            printf("\nEnter number of nodes of the graph:\n");
            scanf("%d", &(graph->n));
            
            printf("\nEnter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters:\n");
            for (int i=0; i<graph->n; i++)
                for (int j=0; j<graph->n; j++)
                    scanf("%d", &(graph->adjmat[i][j]));
                
            shmdt(graph);
        }

        else
        {
            int* start = (int*)shmat(shmid, NULL, 0);

            printf("\nEnter starting vertex:\n");
            scanf("%d", start);

            shmdt(start);
        }

        send_request(msgqid, seqnum, opnum, graphfile);

        Reply reply = recieve_reply(msgqid, seqnum);

        if (opnum == 1 || opnum == 2)
            printf("\n%s\n", reply.mtext);
        
        else if (opnum == 3)
            printf("\nOutput of the DFS traversal is:\n%s\n", reply.mtext);
        
        else
            printf("\nOutput of the BFS traversal is:\n%s\n", reply.mtext);


        delete_shared_memory(shmid);
    }

    exit(0);
}

