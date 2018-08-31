#include "ILSF.h"

#include <stdio.h>
#include <string.h>
#include "LSF.h"

extern unsigned long long tick;

#define LAXITY(a) (a->a_dl-tick-a->et)

extern TCB* _kernel_runtsk;
extern TCB* _kernel_runtsk_pre;
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

unsigned long long migration;

SCHEDULING_ALGORITHM ILSF_sa={
    .scheduling_initialize = ILSF_scheduling_initialize,
    .scheduling = ILSF_Scheduling,
    .insert_OK  = ILSF_insert_OK,
    .reorganize_function = ILSF_reorganize_function,
};

void ILSF_scheduling_initialize()
{
    return;
}

void ILSF_swap(TCB *t1,TCB *t2)
{
    TCB TCB_tmp;

    if(t1==NULL || t2==NULL)
    {
        return;
    }

    memcpy(&TCB_tmp,t1,DIFF(TCB,next));
    memcpy(t1      ,t2,DIFF(TCB,next));
    memcpy(t2,&TCB_tmp,DIFF(TCB,next));
}

void ILSF_sort_ready_queue(TCB **rq)
{
    TCB* p = NULL;
    TCB* q = NULL;
    p = *rq;
    if(rq == NULL)
    {
        return;
    }
    
    // bubble sort for sorting the linkedlist
    for(;p!=NULL;p=p->next)
    {
        for(q=p->next;q!=NULL;q=q->next)
        {
        	if(LAXITY(p)>LAXITY(q))
            {
            	if(LAXITY(q)<THRESHOLD)
            	{
                    ILSF_swap(p,q);
            	}
            }
        }
    }
}

int ILSF_insert_OK(TCB* t1,TCB* t2)
{
   if(t1==NULL)
    {
        return 0;
    }
    if(t2==NULL)
    {
        return 1;
    }
    if(LAXITY(t1)<LAXITY(t2))
    {
        return 1;
    }
    return 0;
}

void ILSF_reorganize_function(TCB** rq)
{
    // sort TCB according to the laxity
    ILSF_sort_ready_queue(rq);
    return;
}

void ILSF_Scheduling()
{
    LSF_Scheduling();
}
