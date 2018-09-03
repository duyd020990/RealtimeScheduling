#include "LSF.h"

#include <stdio.h>
#include <string.h>

#define LSF

#include "Schedule.h"

extern unsigned long long tick;

#define LAXITY(a) (a->a_dl-tick-a->et)

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

/* Variables for overheads estimation */
extern unsigned long long migration;
extern unsigned long long overhead_dl;
extern unsigned long long overhead_dl_max;
extern unsigned long long overhead_dl_total;
extern unsigned long long overhead_alpha;
extern unsigned long long overhead_alpha_max;
extern unsigned long long overhead_alpha_total;


SCHEDULING_ALGORITHM LSF_sa={
    .scheduling_initialize = LSF_scheduling_initialize,
    .scheduling            = LSF_Scheduling,
    .insert_OK             = LSF_insert_OK,
    .reorganize_function   = LSF_reorganize_function,
};

void LSF_scheduling_initialize()
{
    return;
}

void LSF_swap(TCB *t1,TCB *t2)
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

void LSF_sort_ready_queue(TCB **rq)
{
    TCB* p = NULL;
    TCB* q = NULL;
    p = *rq;
    if(rq == NULL)
    {
        return;
    }

    for(;p!=NULL;p=p->next)
    {
        for(q=p->next;q!=NULL;q=q->next)
        {
        	if(LAXITY(p)>LAXITY(q))
            {
                LSF_swap(p,q);
            }
        }
    }
}

int LSF_insert_OK(TCB* t1,TCB* t2)
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

void LSF_reorganize_function(TCB** rq)
{
    // sort TCB according to the laxity
    LSF_sort_ready_queue(rq);
    return;
}

void LSF_Scheduling()
{
    int i;
    TCB* p = NULL;
    TCB* ap = NULL;

    p = p_ready_queue;
    ap = ap_ready_queue;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(p == NULL)
        {
            _kernel_runtsk[i] = ap;
            if(ap!=NULL){ap = ap->next;}
        }
        else if(ap == NULL)
        {
            _kernel_runtsk[i] = p;
            if(p!=NULL){p = p->next;}
        }
        else
        {
            if(p->a_dl <= ap->a_dl)
            {
                _kernel_runtsk[i] = p;
                if(p!=NULL){p = p->next;}
            }
            else
            {
                _kernel_runtsk[i] = ap;
                if(ap!=NULL){ap = ap->next;}
            }
        }

        if(_kernel_runtsk[i] != _kernel_runtsk_pre[i])
        {
            migration++;
            _kernel_runtsk_pre[i] = _kernel_runtsk[i]; 
        }
    }
}