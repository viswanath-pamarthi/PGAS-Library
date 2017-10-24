#ifndef PGAS_H_INCLUDED
#define PGAS_H_INCLUDED

#include<mpi.h>
#include <pthread.h>
#include <stdio.h>
#include<stdlib.h>

#define PGAS_SUCCESS    0
#define PGAS_ERROR     -1
#define PGAS_NO_SPACE  -2


MPI_Comm world_comm,dup_comm_world;




//tag 0 - stop the server
typedef MPI_Aint PGAS_HANDLE[3];
void *threadFunc();
int PGAS_Init(int max_bytes);
int PGAS_Fadd_init(PGAS_HANDLE handle, int init_val);
int PGAS_Fadd(PGAS_HANDLE handle,  int incr_val, int *old_val);
int PGAS_Locks_init(int num_locks);
int PGAS_Lock(int lock_num);
int PGAS_Unlock(int lock_num);
int PGAS_Finalize();
#endif

