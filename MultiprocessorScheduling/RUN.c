#include "RUN.h"

#include "Schedule.h"
#include "EDF.h"

#include <stdio.h>


extern int has_new_task; 
extern int has_new_instance;

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

/* 
  This running queue for those Multiprocessor Real-time Scheduling,
  Since the migration only occurse in Multiproccessor platform
 */
TCB *running_queue[PROCESSOR_NUM]; // A running queue for each processor


SCHEDULING_ALGORITHM RUN_sa = {
    .scheduling_initialize = RUN_scheduling_initialize,
    .scheduling            = RUN_schedule,
    .insert_OK             = RUN_insert_OK,
    .reorganize_function   = RUN_reorganize_function
};

void RUN_scheduling_initialize()
{
    return;
}

int RUN_insert_OK(TCB* t1,TCB* t2)
{
    return EDF_insert_OK(t1,t2);
}


void RUN_reorganize_function(TCB** rq)
{

}


/*
Architecture of Scheduling function for RUN algorithm

RUNScheduling:
if has new task:
    build reduction tree
    search run
else if has new instance:
    update reduction tree
    search run
run scheduling
*/
void RUN_schedule()
{
    
}