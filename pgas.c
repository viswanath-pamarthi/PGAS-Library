#include "pgas.h"
#include <pthread.h>
#include <stdio.h>
#include<stdlib.h>


MPI_Comm world_comm,dup_comm_world;
long int max_mem_bytes;
long int availableMem;
int runThread=1;// To let thread receive break while loop on0 in finalize
int rankServer;
int comSize;




struct lockWaitList{
	int rank;
	struct lockWaitList *next;
};

struct lock {
  int x;
  int count;//number of waits for this lock
  int locked;
  struct lock *next;
  struct lockWaitList *list;
};


struct lock *locks;    
//tag 0 - stop the server
int totalLocks=0;
void *threadFunc()
{
	int i=0,rank, comm_size;

	MPI_Comm_rank(dup_comm_world, &rank);
	MPI_Comm_size(dup_comm_world, &comm_size);
	
	MPI_Status status;
	int source,tag,countSize;
	while(runThread==1)
	{
		int ierr=MPI_Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,dup_comm_world, &status);
		source=status.MPI_SOURCE;tag=status.MPI_TAG;

		//Stop the thread call from PGAS Finalize
		if(tag==0)
		{
			MPI_Recv(NULL,0,MPI_INT, source,tag, dup_comm_world,&status);
			runThread++;		
		}

		if(tag==1)//Fadd Init
		{			
			int data;int* memPointer;
			PGAS_HANDLE reply;
										
			memPointer=malloc(sizeof(int));																											
			availableMem= availableMem - sizeof(int);									
			MPI_Recv(memPointer,1,MPI_INT, source, tag, dup_comm_world, &status);
			reply[0]=rank;reply[1]=(MPI_Aint)memPointer;  
			reply[2]=sizeof(int);		

			MPI_Rsend(&reply, 3, MPI_AINT, source, 2, dup_comm_world);		
									

		}

				
		if(tag==5)//Fadd
		{
			
			MPI_Aint infoSource[5];int oldVal;
			MPI_Recv(&infoSource, 5, MPI_AINT, source, tag, dup_comm_world, &status);
			
			int* retrieveData=(int*)infoSource[1];
			oldVal=retrieveData[0];					
			retrieveData[0]=retrieveData[0]+infoSource[4];			
			
			MPI_Rsend(&oldVal, 1, MPI_INT, source, 6, dup_comm_world);
						
		}

		if(tag==3)//PGAS_Lock_init
		{
			int val;
			MPI_Recv(&val,1,MPI_INT, source, tag, dup_comm_world, &status);
			
			if(locks==NULL)
			{				
					locks=malloc( sizeof(struct lock) );
					locks->next=NULL;
					locks->list=NULL;
					locks->x=val;
					locks->locked=0;
					locks->count=0;
			}
			else
			{
				struct lock *temp;  
				temp=locks;
				while(temp->next!=NULL)
				{
					temp=temp->next;
				}
				
				struct lock *newLock=malloc( sizeof(struct lock) );
				newLock->next=NULL;
				newLock->list=NULL;
				newLock->x=val;
				newLock->locked=0;
				newLock->count=0;
				
				temp->next=newLock;
				
			}
			
			
		}

		if(tag==4)//locking
		{
			int val;struct lock *temp;  int success=1;struct lock *temp2;
			MPI_Recv(&val,1,MPI_INT, source, tag, dup_comm_world, &status);
			temp=locks;
			if(source==1000)
			{
				
				temp2=locks;
					while(temp2!=NULL)
				{
					printf("%d is lock from server %d\n",temp2->x, rankServer);
					temp2=temp2->next;
				}
				
			}
			while(temp!=NULL)
			{
				
				if((temp->x) == val)
				{
					
					
					if(temp->locked==0)//not locked
					{
						
						temp->locked=1;
						totalLocks++;
						MPI_Rsend(&success, 1, MPI_INT, source, 7, dup_comm_world);
						
					}
					else//already locked
					{
						struct lockWaitList *newWait=malloc( sizeof(struct lockWaitList) );
						newWait->rank=source;
						newWait->next=NULL;
						
						if((temp->list)==NULL)
						{
							temp->list=newWait;
							
						}
						else
						{
							struct lockWaitList *temp1;  
							temp1=temp->list;
							while(temp1->next!=NULL)
							{
								temp1=temp1->next;
							}
							temp1->next=newWait;
						}
					}
					
				}
				
				temp=temp->next;				
			}
			
			
		}
		
		if(tag==8)//unlock
		{
			int val;struct lock *temp;  int success=1;
			MPI_Recv(&val,1,MPI_INT, source, tag, dup_comm_world, &status);
			temp=locks;
			
			while(temp!=NULL)
			{
				if(temp->x == val)
				{
					if(temp->locked==1)//locked
					{
						if(temp->list==NULL)
						{
							temp->locked=0;
							totalLocks--;
						}
						else{
							
							int rankToUnblock;
							rankToUnblock=temp->list->rank;
							temp->list=temp->list->next;
														
							MPI_Rsend(&success, 1, MPI_INT, rankToUnblock, 7, dup_comm_world);							
						}
					}
						
				}				
				temp=temp->next;
				
			}
			
		}		
					
	}

	pthread_exit(0);
}

int PGAS_Init(int max_bytes)
{
	int flag,provided,size,rank;

	MPI_Initialized(&flag);
	
	if (!flag)
	{
		MPI_Init_thread(0, 0, MPI_THREAD_MULTIPLE, &provided );
	}
	
	max_mem_bytes=max_bytes;
	availableMem=max_bytes;
	
	MPI_Comm_dup( MPI_COMM_WORLD, &dup_comm_world );

	MPI_Comm_rank(dup_comm_world, &rankServer);
    MPI_Comm_size(dup_comm_world, &comSize);
	

	int j=rank;pthread_t id;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&id,NULL,threadFunc,(void*)&j);//Create Threads

	pthread_attr_destroy(&attr);	

	return 0;


}


int PGAS_Fadd_init(PGAS_HANDLE handle, int init_val)
{
	

	MPI_Request request;MPI_Status status;int i;
	int serverTosend;
	serverTosend=rankServer%comSize;
	
	MPI_Irecv(handle, 3, MPI_AINT, serverTosend, 2, dup_comm_world, &request);
	MPI_Send(&init_val, 1, MPI_INT, serverTosend, 1, dup_comm_world);
	MPI_Wait(&request,&status);
		 
	return 0;		

}

int PGAS_Fadd(PGAS_HANDLE handle,  int incr_val, int *old_val)
{	

	MPI_Request request;MPI_Status status;
	MPI_Aint tempHandle[5];

	tempHandle[0]=handle[0];tempHandle[1]=handle[1];tempHandle[2]=handle[2];tempHandle[3]=0;tempHandle[4]=incr_val;
	
	MPI_Irecv(old_val, 1, MPI_INT, handle[0], 6, dup_comm_world, &request);
	MPI_Send(&tempHandle, 5, MPI_AINT, handle[0], 5, dup_comm_world);
	MPI_Wait(&request,&status);	

	return 0;

}

int PGAS_Locks_init(int num_locks){
	int locksToHandle,start,i,rankToStore;
	locksToHandle=(num_locks-(num_locks%comSize))/comSize;
	start=locksToHandle*rankServer;

	if(((num_locks%comSize) >0) &&(rankServer==(comSize-1)))
	{
		locksToHandle=locksToHandle+	(num_locks%comSize);			
	}

	i=0;
	
	while(i<locksToHandle)
	{
		
		rankToStore=start%comSize;
		
		MPI_Send(&start, 1, MPI_INT, rankToStore, 3, dup_comm_world);		
		i++;
		start++;
	}
	
	MPI_Barrier(dup_comm_world);

	return 0;
}

int PGAS_Lock(int lock_num){	
	int serverToAsk;int result=0;
	MPI_Request request;MPI_Status status;
	
	serverToAsk=(lock_num%comSize);
	
	MPI_Irecv(&result, 1, MPI_INT, serverToAsk, 7, dup_comm_world, &request);
	MPI_Send(&lock_num, 1, MPI_INT, serverToAsk, 4, dup_comm_world);
	MPI_Wait(&request,&status);
	
	if(result==1)
	{
		return 0;
	}
	else	
	{
		return -1;
	}
	
}

int PGAS_Unlock(int lock_num){
	int serverToAsk;
	serverToAsk=(lock_num%comSize);
	MPI_Send(&lock_num, 1, MPI_INT, serverToAsk, 8, dup_comm_world);
}


int PGAS_Finalize()
{
	MPI_Barrier(dup_comm_world);	
	MPI_Send(NULL,0,MPI_INT,rankServer ,0,dup_comm_world);	  

}

