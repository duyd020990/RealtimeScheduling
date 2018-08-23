#include "LLREF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Schedule.h"
#include "LSF.h"

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

extern unsigned long long period[MAX_TASKS];
extern unsigned long long tick;

int release_time[TICKS];
int assign_history[MAX_TASKS];

unsigned long long time_interval;

LLREF_LRECT* lrect_queue;

// This running queue for those Multiprocessor Real-time Scheduling,
// Since the migration only occurse in Multiproccessor platform
TCB *running_queue[PROCESSOR_NUM]; // A running queue for each processor

SCHEDULING_ALGORITHM LLREF_sa={
    .scheduling          = LLREF_scheduling,
    .insert_OK           = LLREF_insert_OK,
    .reorganize_function = LLREF_reorganize_function
};


/*
  Here we will use the tick and array release_time
*/
unsigned long long local_remaining_execution_time(const TCB* tcb)
{
    unsigned long long lrect = 0;
    unsigned long long tick_tmp = 0;

    for(tick_tmp = tick+1;!release_time[tick_tmp];tick_tmp++);

    time_interval = tick_tmp - tick;
    lrect = tcb->et*(time_interval)/period[tcb->tid];

    return lrect;
}

/*
  Sort the waiting queue according to the local remainning execution time.
*/

void LLREF_swap(LLREF_LRECT *rl1,LLREF_LRECT *rl2)
{
    LLREF_LRECT LRECT_tmp;

    if(rl1==NULL || rl2==NULL)
    {
        return;
    }

    memcpy(&LRECT_tmp,rl1,DIFF(LLREF_LRECT,next));
    memcpy(rl1       ,rl2,DIFF(LLREF_LRECT,next));
    memcpy(rl2,&LRECT_tmp,DIFF(LLREF_LRECT,next));
}

void LLREF_lrect_queue_sort(LLREF_LRECT** rl)
{
    LLREF_LRECT* p = NULL;
    LLREF_LRECT* q = NULL;

    if(rl == NULL)
    {
        return;
    }  

    p = *rl;
    for(;p!=NULL;p=p->next)
    {
        for(q=p->next;q!=NULL;q=q->next)
        {
            if((p->local_remaining_execution_time)<(q->local_remaining_execution_time))
            {
                LLREF_swap(p,q);
            }
        }
    }
}

void LLREF_lrect_queue_build(TCB** rq)
{
    TCB*         p  = NULL;
    LLREF_LRECT* ll = NULL;

    if(rq==NULL || *rq==NULL)
    {
        return;
    }

    for(p=*rq;p!=NULL;p=p->next)
    {
        ll = (LLREF_LRECT*)malloc(sizeof(LLREF_LRECT));
        if(ll == NULL)
        {
            fprintf(stderr,"Error in malloc\n");
            return;
        }
        ll->local_remaining_execution_time = local_remaining_execution_time(p);
        ll->tcb = p;
        ll->next = NULL;

        // Insert new ll into lrect_queue, 
        // later we will sort this lrect_queue,
        // so the order how we insert the elelment is doesn't matter.
        ll->next = lrect_queue;
        lrect_queue = ll;
    }

    LLREF_lrect_queue_sort(&lrect_queue);
}

void LLREF_reorganize_function(TCB** rq)
{
    if(rq == NULL || *rq == NULL)
    {
        return;
    }
}


int LLREF_insert_OK(TCB* t1,TCB* t2)
{
    return 0;
}

int LLREF_assign_history_check(TCB* tcb)
{
    return assign_history[tcb->tid];
}

void LLREF_scheduling()
{    
    int i;
    int processor_id=-1;
    int assigned[PROCESSOR_NUM] = {0};
    TCB *p          = NULL;
    TCB *ap         = NULL;
    LLREF_LRECT* ll = NULL;


    time_interval--;
    
    if(time_interval>0)
    {
        return;
    }

    //lrect_queue is only for periodic tasks,so it might be NULL 
    LLREF_lrect_queue_build(&p_ready_queue);
    ll = lrect_queue;
    for(;ll!=NULL;ll=ll->next)
    {
        p = ll->tcb;processor_id = LLREF_assign_history_check(p);
        if(processor_id == -1)
        {
            // Search a available processor
            for(processor_id=0;processor_id<PROCESSOR_NUM;processor_id++)
            {
                if(!assigned[processor_id]){break;}
            }

            if(processor_id<PROCESSOR_NUM)
            {
                _kernel_runtsk[processor_id] = p;
                if(ll!=NULL){ll = ll->next;}
                else{ll = NULL;}
                assigned[processor_id]=1;
            }
        }
        if(_kernel_runtsk_pre[processor_id] != _kernel_runtsk[processor_id])
        {
            _kernel_runtsk_pre[processor_id] = _kernel_runtsk[processor_id];
        }
    }

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(!assigned[i])
        {
            _kernel_runtsk[i] = ap;
            if(ap!=NULL){ap = ap->next;}
            else{ap = NULL;}
            assigned[i]=1;

            if ( _kernel_runtsk[i] != _kernel_runtsk_pre[i] ) 
            {
                _kernel_runtsk_pre[i] = _kernel_runtsk[i];
            } 
        }
    }
    
}