#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/time.h>
#include <unistd.h>
//using namespace std;

#define MAX_NODES 30
#define MAX_STACK_SIZE 2000
#define MAX_COST 0x0fffffffff;
long **matrix = NULL;
int numProcess = 10;
int numVertex = 12;
int taskLen = 3;
long gMinCost = 333333;
float impFactor = 1;
long minCostTimer = 1250;
char fileName[255] = {0};
long greedyCost = 0;
MPI_Datatype matrixType;
long visitedNodes = 0;
int compGraph = 1;
int seed = 10;
//int* matrix;


typedef struct
{
	int *arr;
	long cost;
	int pos;
	int prevVertex;
}sNode;

sNode *stack;
int top = -1;

enum
{
	START_TASK = 1,
	M_TO_W_MIN,
	W_TO_M_MIN,
	TASK_DONE,
	RECV_TIME,
	PATHS_TAKEN
};

FILE *openFile(const char *fileName, const char *mode)
{
	FILE *fp;
	fp = fopen(fileName, mode);
	if(fp == NULL)
	{
		fprintf(stderr, "Error: Could not open file '%s'\n", fileName);
		exit(-1);
	}

	return fp;
}

void *doMalloc(long size)
{
	void *p = (void *) malloc(size);
	if(p == NULL)
	{
		fprintf(stderr, "Error: Malloc failed\n");
		exit(-1);
	}
	return p;
}

long getCurTime()
{
	struct timeval tv;
	long curTime, endTime;

	gettimeofday(&tv, NULL);
	curTime = tv.tv_sec * 1000000 + tv.tv_usec;
	return curTime;
}

long totSendTime = 0;
long totRecvTime = 0;
long totSendCount = 0;
long minSendCount = 0;
long minRecvCount = 0;
void sendData(const void *buf, int count, MPI_Datatype datatype, 
			int dest, int tag, MPI_Comm comm)
{
	
	long curTime, endTime;

	curTime = getCurTime();
	MPI_Send(buf, count, datatype, dest, tag, comm);
	totSendTime += (getCurTime() - curTime);
	totSendCount++;
}

void recvData(void *buf, int count, MPI_Datatype datatype,
			int source, int tag, MPI_Comm comm, MPI_Status *status)
{
	long curTime, endTime;

	curTime = getCurTime();
	MPI_Recv(buf, count, datatype, source, tag, comm, status);
	totRecvTime += (getCurTime() - curTime);
}

void initSNode(sNode *s)
{
	s->arr = (int *)doMalloc(sizeof(int) * numVertex);
	memset(s->arr, 0, sizeof(int) * numVertex);
}

void mallocStack()
{
	stack = (sNode *) doMalloc(MAX_STACK_SIZE * sizeof(sNode));
	for(int i = 0; i < MAX_STACK_SIZE; i++)	
	{
		initSNode(stack + i);
	}
}


void push(sNode *s)
{
	#if 0
	printf("push   ");
	for(int i = 0; i < numVertex; i++)
		printf("%5d", s->arr[i]);
	printf("\n");
	#endif

	top++;
	if(top >= MAX_STACK_SIZE)
	{
		fprintf(stderr, "Stack overflow\n");
		exit(-1);
	}

	memcpy(stack[top].arr, s->arr, sizeof(int)*numVertex);
	stack[top].cost = s->cost;
	stack[top].pos = s->pos;
	stack[top].prevVertex = s->prevVertex;
}

int pop(sNode *s)
{
	if(top == -1)	return -1;

	memcpy(s->arr, stack[top].arr, sizeof(int)*numVertex);
	s->cost = stack[top].cost;
	s->pos = stack[top].pos;
	s->prevVertex = stack[top].prevVertex;
	top--;
	#if 0
	printf("pop    ");
	for(int i = 0; i < numVertex; i++)
		printf("%5d", s->arr[i]);
	printf("\n");
	#endif
	return 0;
}

void fillTask(int *task, int taskLen)
{
	for(int i = 0; i < taskLen; i++)	task[i] = i;
}

void constructMatrixType(int n)
{
	MPI_Type_contiguous(n*n,MPI_INT,&matrixType);
	MPI_Type_commit(&matrixType);
}

long worker(int *task, int taskLen)
{
	int minArr[numVertex + 1] = {0};

	long rootCost = 0;
	//printf("Root cost = %d\n", rootCost);

	long minCost = gMinCost;
	long msg = 0;
        MPI_Send(&msg, 1, MPI_LONG_LONG_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD);
        MPI_Recv(&msg, 1, MPI_LONG_LONG_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(minCost > msg) minCost = msg;

	for(int i = 0; i < taskLen - 1; i++)
	{
		//Check if path exists
		if(matrix[task[i]][task[i+1]] == 0)	return minCost;
		rootCost += matrix[task[i]][task[i+1]];
	}
	if(rootCost >= minCost) return minCost;

	long getMinCount = 0;
	static sNode tmpNode = {0};
	if(tmpNode.arr == NULL) initSNode(&tmpNode);
	else memset(tmpNode.arr, 0, sizeof(int)*numVertex);

	tmpNode.pos = 1;
	for(int i = 0; i < taskLen; i++)
	{
		int vertex = task[i];
		tmpNode.arr[vertex] = tmpNode.pos++;
	}

	tmpNode.cost = rootCost;
	tmpNode.prevVertex = task[taskLen - 1];

	while(1)
	{
		int c = 0;
		for(int i = 0; i < numVertex; i++)
		{
			if(tmpNode.arr[i] == 0)
			{

				//Check if cost of current sub path is not exceeding current min
				long cost = tmpNode.cost + matrix[tmpNode.prevVertex][i];
				if(cost >= minCost || cost == tmpNode.cost)	continue;

				visitedNodes++;

				int prevVertex = tmpNode.prevVertex;
				//Fill values in new node
				tmpNode.pos += 1;
				tmpNode.cost += matrix[tmpNode.prevVertex][i];
				tmpNode.arr[i] = tmpNode.pos;
				tmpNode.prevVertex = i;

				//Push the node
				push(&tmpNode);

				//Clear the values
				tmpNode.arr[i] = 0;
				tmpNode.cost -= matrix[prevVertex][i];
				tmpNode.pos -= 1;
				tmpNode.prevVertex = prevVertex;
			}
		}

		if(pop(&tmpNode) != 0)	break;

		c = 1;
		for(int i = 0; i < numVertex; i++)
		{
			if(tmpNode.arr[i] == 0)	{ c = 0; break; }
		}

		if(c == 1)
		{
			//Complete the circuit
			if(matrix[tmpNode.prevVertex][0] == 0)	return minCost;

			tmpNode.cost += matrix[tmpNode.prevVertex][0];
			if(tmpNode.cost < minCost)
			{
				minCost = tmpNode.cost;
				memcpy(minArr, tmpNode.arr, sizeof(int) * numVertex);
				MPI_Send(&minCost, 1, MPI_LONG_LONG_INT, 0, W_TO_M_MIN, MPI_COMM_WORLD);
			}
		}
		getMinCount++;
		
		if(getMinCount == minCostTimer || getMinCount == minCostTimer+1000)
		//if(0)
		{
			long msg = 0;
			static int flag = 0;
			if(flag == 0)
			{
				flag = 1;
        			MPI_Send(&msg, 1, MPI_LONG_LONG_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD);
			}

			else
			{
        			recvData(&msg, 1, MPI_LONG_LONG_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        			if(minCost > msg) minCost = msg;
				getMinCount = 0;
				flag = 0;
			}
		}
		//Receive minCost from master
		#if 0
		int msg = 0;
		MPI_Send(&msg, 1, MPI_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD);
		MPI_Recv(&msg, 1, MPI_INT, 0, M_TO_W_MIN, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
		if(minCost > msg) minCost = msg;
		#endif
	}

	//Get min array
	int tmpArr[numVertex] = {0};
	for(int i = 0; i < numVertex; i++)
		tmpArr[minArr[i]-1] = i;

	#if 0
	printf("Min Arr = \n");
	for(int i = 0; i < numVertex; i++)
		printf("%4d", minArr[i]);
	printf("\n");
	#endif

	//if(minCost < gMinCost)
	if(0)
	{
		printf("Min cost = %ld\n", minCost);
		for(int i = 0; i < numVertex; i++)
		{
			printf("%4d", tmpArr[i]);
        
			if(i < numVertex - 1)
				printf(" --(%ld)-->", matrix[tmpArr[i]][tmpArr[i+1]]);
		}
		//Print last vertex to source
		printf("  --(%ld)-->    0", matrix[tmpArr[numVertex-1]][0]);
		printf("\n");
	}

	return minCost;
}

void WWE(int rank)
{
	long msg = 0;
	int task[taskLen] = {0};
	while(1)
	{
		//Send request for START_TASK
		MPI_Send(&msg, 1, MPI_LONG_LONG_INT, 0, START_TASK, MPI_COMM_WORLD);
		MPI_Recv(task, taskLen, MPI_LONG_LONG_INT, 0, START_TASK, MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
		if(task[0] == -1)  
		{
			//printf("No task left. Leaving... rank = %d\n", rank);
			break;
		}
		
		long minValue = worker(task, taskLen);
		if(minValue < gMinCost)
		{
			MPI_Send(&minValue, 1, MPI_LONG_LONG_INT, 0, W_TO_M_MIN, MPI_COMM_WORLD);
		}

		//Send number of visited nodes
		MPI_Send(&visitedNodes, 1, MPI_LONG_LONG_INT, 0, PATHS_TAKEN, MPI_COMM_WORLD);
		MPI_Send(&msg, 1, MPI_LONG_LONG_INT, 0, RECV_TIME, MPI_COMM_WORLD);

		//Send total time spent in receiving data
		MPI_Send(&totRecvTime, 1, MPI_LONG_LONG_INT, 0, RECV_TIME, MPI_COMM_WORLD);
		MPI_Send(&minValue, 1, MPI_LONG_LONG_INT, 0, TASK_DONE, MPI_COMM_WORLD);

		//Reset values
		visitedNodes = 0;
		totRecvTime = 0;
	}

	
}

void master(int size)
{
	long numTask = (numVertex - 1) * (numVertex - 2);
	int taskList[numTask][taskLen] = {0};
	long rootCost[numTask] = {0};
	long numPaths = 0;
	long tmp = 0;

	int i = 0, j = 1, k = 2;
	for(i = 0; i < numTask; i++)
	{
		taskList[i][0] = 0;
		taskList[i][1] = j;
		taskList[i][2] = k++;

		if(j == k) k++;
		if(k == numVertex) {  k = 1; j++; }

		int n1 = taskList[i][0];
		int n2 = taskList[i][1];
		int n3 = taskList[i][2];
		rootCost[i] = matrix[n1][n2] + matrix[n2][n3];
	}

	long msgFromWorker;
	MPI_Status mpiStatus;
	int curTask = 0;
	long taskDone = 0;
	long exitTask[taskLen];
	exitTask[0] = -1;

	while(taskDone < numTask)
	{
		//Receive msg
		MPI_Recv(&msgFromWorker, 1, MPI_LONG_LONG_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpiStatus); 
		switch(mpiStatus.MPI_TAG)
		{
			case START_TASK:
				if(curTask < numTask)
				{
					sendData(taskList[curTask], taskLen, MPI_INT, 
							mpiStatus.MPI_SOURCE, START_TASK, MPI_COMM_WORLD);
					curTask++;
				}
				break;

			case W_TO_M_MIN:
				if(gMinCost > msgFromWorker)
				{
				       minRecvCount++;
				       gMinCost = msgFromWorker;	
				}
				break;

			case M_TO_W_MIN:
				//send gMinCost
				sendData(&gMinCost, 1, MPI_LONG_LONG_INT, 
						mpiStatus.MPI_SOURCE, M_TO_W_MIN, MPI_COMM_WORLD);
				minSendCount++;
				break;

			case TASK_DONE:
				//Mark task done
				printf("--- Task %ld done, minCost = %ld ---\n", taskDone, gMinCost);
				fflush(stdout);
				taskDone++;
				break;
			case RECV_TIME:
				MPI_Recv(&tmp, 1, MPI_LONG_LONG_INT, mpiStatus.MPI_SOURCE, RECV_TIME, MPI_COMM_WORLD, &mpiStatus); 
				totRecvTime += tmp;
				break;
			case PATHS_TAKEN:
				numPaths += msgFromWorker;
				break;
		}
	}

	for(int i = 1; i < size; i++) 
		sendData(exitTask, taskLen, MPI_LONG_LONG_INT, i, START_TASK, MPI_COMM_WORLD);

	printf("Master is leaving.\n");
	printf("MasterNumPaths=%ld,masterMinCost=%ld\n", numPaths, gMinCost);
	printf("totSendTime = %ld, totRecvTime = %ld, totSendCount = %ld, "
		"minSendCount = %ld, minRecvCount = %ld\n", 
			totSendTime/1000000+1, totRecvTime/1000000+1, totSendCount, minSendCount, minRecvCount);
	fflush(stdout);
}


void mallocMatrix()
{
	matrix = (long **)doMalloc(sizeof(void *) * numVertex);
	for(int i = 0; i < numVertex; i++)
	{
		matrix[i] = (long *)doMalloc(sizeof(long) * numVertex);
		memset(matrix[i], 0, sizeof(long) * numVertex);
	}
}

void printMatrix()
{
	printf("---------------------------------\n");
	for(int i = 0; i < numVertex; i++)
	{
		for(int j = 0; j < numVertex; j++)
			printf("%5ld", matrix[i][j]);

		printf("\n");
	}
	printf("---------------------------------\n");
}

int getTokens(char buf[], char *tokens[])
{
	char *token;
	int i = 0;

	token = strtok(buf, ",");
	while(token != NULL)
	{
		tokens[i++] = token;
		token = strtok(NULL, ",");
	}
	return i;
}

void readMatrix(char *fileName)
{
	FILE* fp;
	int i, j;
	char *tokens[64] = {0};
	char buf[1024] = {0};

	fp = openFile(fileName, "r");
	for(i = 0; i < numVertex; i++)
	{
		if(fgets(buf, 1024, fp) == NULL)
		{
			fprintf(stderr, "Error in input file\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}
		if(getTokens(buf, tokens) != numVertex)
		{
			fprintf(stderr, "Error in input line\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
		}

		for(j = 0; j < numVertex; j++)
			matrix[i][j] = atoi(tokens[j]);
	}
	
	fclose(fp);
}

void fillMatrix()
{
	srand(seed);

	for(int i = 0; i < numVertex; i++){
		for(int j = 0; j < numVertex; j++){
			if(i != j){
				matrix[i][j] = rand() % 1000 + 1;
				//staticMatrix[i][j] = matrix[i][j];
			}
		}
	}
}

void writeMatrix(char *fileName)
{
	FILE *fp = openFile(fileName, "w");
	long value;

	srand(seed);

	for(int i = 0; i < numVertex; i++)
	{
		for(int j = 0; j < numVertex; j++)
		{
			if(i == j) value = 0;
			else       value = rand() % 1000 + 1;

			if(!compGraph && value > 900)	value = 0;

			if(j == numVertex - 1)  fprintf(fp, "%ld\n", value);
			else			fprintf(fp, "%ld,", value);
		}
	}
	fclose(fp);
}

int findMin(long *arr, int len, int *visited)
{
	int min = -1;

	for(int i = 1; i < len; i++)
	{
		if(visited[i] == 0)
		{
			if(min == -1) min = i;
			else if((arr[i] > 0) && (arr[i] < arr[min] || arr[min] == 0)) min = i;
		}
	}

	if(arr[min] == 0) min = -1;
	return min;
}

void findGreedyCost(int rank)
{
	long greedyCost = 0;
	int visited[numVertex] = {0};

	int src = 0;
	int next;
	for(int i = 0; i < numVertex-1; i++)
	{
		visited[src] = 1;
		next = findMin(matrix[src], numVertex, visited);
		#if 0
		if(next == -1)
		{
			fprintf(stderr, "Error: min value -1\n");
			exit(-1);
		}
		#endif
		if(next == -1 || matrix[src][next] == 0)	
		{
			greedyCost = MAX_COST;
			gMinCost = greedyCost;
			printf("Greedy cost = %ld by rank = %d\n", greedyCost, rank);
			return;
			//matrix[src][next] = rand() % 1000 + 1;
		}
		
		greedyCost += matrix[src][next];
		src = next;
	}

	greedyCost += matrix[src][0];
	greedyCost *= impFactor;
	gMinCost = greedyCost;
	printf("Greedy cost = %ld by rank = %d\n", greedyCost, rank);
	fflush(stdout);
}


void parseArgs(int argc, char *argv[], int rank)
{
	int opt;
	while ((opt = getopt(argc, argv, "f:v:i:s:c")) != -1) 
	{
		switch (opt) 
		{
			case 'f':
			strcpy(fileName, optarg);
			break;

			case 'v':
			numVertex = atoi(optarg);
			break;

			case 'i':
			minCostTimer = atoi(optarg);
			break;

			case 's':
			seed = atoi(optarg);
			break;

			case 'c':
			compGraph = 0;
			break;

			default:
                   	fprintf(stderr, "Error: Invalid arg\n");
			MPI_Abort(MPI_COMM_WORLD, 0);
               }
	}

	if(numVertex == 0)
	{
		fprintf(stderr, "Error: Please provide numVertex\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	if(fileName[0] == 0)
	{
		sprintf(fileName, "/tmp/matrixFile_%d.csv", rank);
		writeMatrix(fileName);
	}
}

void bCastMatrix(int rank)
{
	int k = 0;
	long *m = (long *)doMalloc(numVertex * numVertex * sizeof(long));

	if(rank == 0)
	{
		for(int i = 0; i < numVertex; i++)
			for(int j = 0; j < numVertex; j++)
				m[k++] = matrix[i][j];
	}

	int ret = MPI_Bcast(m, numVertex*numVertex, MPI_LONG_LONG_INT, 0, MPI_COMM_WORLD);
	if(ret != MPI_SUCCESS)
	{
		fprintf(stderr, "Error: Could not broadcast matrix\n");
		MPI_Abort(MPI_COMM_WORLD, 0);
	}

	k = 0;
	if(rank != 0)
	{
		for(int i = 0; i < numVertex; i++)
			for(int j = 0; j < numVertex; j++)
				matrix[i][j] = m[k++];
	}

	if(m) free(m);
}

int main(int argc, char *argv[])
{
	int rank;
	int size;

	//Initialize MPI
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	
	parseArgs(argc, argv, rank);
	mallocMatrix();
	//if(rank == 1)	readMatrix(fileName);
	readMatrix(fileName);
	//constructMatrixType(numVertex);
	//bCastMatrix(rank);
	findGreedyCost(rank);

	//Allocate stack for sub-tasks
	mallocStack();

	if(rank == 0) 	master(size);
	else		WWE(rank);

	MPI_Finalize();
	return 0;
}
