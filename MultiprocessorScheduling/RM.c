#include "RM.h"

#include "Schedule.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern unsigned long long period[MAX_TASKS];
extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

extern unsigned long long overhead_dl;
extern unsigned long long overhead_dl_max;
extern unsigned long long overhead_dl_total;
extern unsigned long long overhead_alpha;
extern unsigned long long overhead_alpha_max;
extern unsigned long long overhead_alpha_total;


SCHEDULING_ALGORITHM RM_sa={
    .scheduling_initialize = RM_scheduling_initialize,
    .scheduling_exit       = RM_scheduling_exit,
    .scheduling            = RM_Scheduling,
    .insert_OK             = RM_insert_OK,
    .reorganize_function   = RM_reorganize_function,
};

void RM_scheduling_initialize()
{
    return;
}

int RM_insert_OK(TCB* t1,TCB* t2)
{
    overhead_dl += COMP;
    if(t1==NULL)
    {
        return 0;
    }
    overhead_dl += COMP;
    if(t2==NULL)
    {
        return 1;
    }
    overhead_dl += COMP;
    if(period[t1->tid]<period[t2->tid])
    {
        return 1;
    }
    return 0;
}

void RM_reorganize_function(TCB **rq)
{
    return;
}

void RM_Scheduling ( void )
{
    int i;
    TCB *p = p_ready_queue;
    TCB *ap = ap_ready_queue;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;   
        if ( p == NULL )
        {
            overhead_dl += COMP;
            _kernel_runtsk[i] = ap;
            if(ap!=NULL){ap = ap->next;}
            else{ap = NULL;}
        }
        else
        {
            overhead_dl += COMP;
            _kernel_runtsk[i] = p;
            if(p!=NULL){p = p->next;}
            else{ p = NULL;}
        }

        overhead_dl += COMP;
        if ( _kernel_runtsk[i] != _kernel_runtsk_pre[i] ) 
        {
            _kernel_runtsk_pre[i] = _kernel_runtsk[i];
        } 
    }
    
  return;
}

void RM_scheduling_exit(){}