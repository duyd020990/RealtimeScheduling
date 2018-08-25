#include "LLREF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Schedule.h"
#include "EDF.h"

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

unsigned long long next_release_time()
{
    unsigned long long tick_tmp = 0;

    for(tick_tmp = tick+1;!release_time[tick_tmp];tick_tmp++);

    return tick_tmp - tick;
}
/*
  Here we will use the tick and array release_time
*/
double local_remaining_execution_time(const TCB* tcb,unsigned long long next_release_time)
{
    if(tcb == NULL){return 0;}

    return ((double)tcb->et*(next_release_time))/period[tcb->tid];
}

/*
  Sort the waiting queue according to the local remainning execution time.
*/

void LLREF_swap(LLREF_LRECT *rl1,LLREF_LRECT *rl2)
{
    LLREF_LRECT LRECT_tmp;

    if(rl1==NULL || rl2==NULL){return;}

    memcpy(&LRECT_tmp,rl1,DIFF(LLREF_LRECT,next));
    memcpy(rl1       ,rl2,DIFF(LLREF_LRECT,next));
    memcpy(rl2,&LRECT_tmp,DIFF(LLREF_LRECT,next));
}

void LLREF_lrect_queue_sort(LLREF_LRECT** rl)
{
    LLREF_LRECT* p = NULL;
    LLREF_LRECT* q = NULL;

    if(rl==NULL){return;}  

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

void lrect_queue_empty()
{
    LLREF_LRECT* ll = NULL;
    LLREF_LRECT* ll_tmp = NULL;

    // Empty lrect_queue
    for(ll = lrect_queue;ll!=NULL;)
    {
        ll_tmp = ll;
        ll = ll->next;
        free(ll_tmp);
    }
    lrect_queue = NULL;
}

void LLREF_lrect_queue_build(TCB** rq)
{

    TCB*         p  = NULL;
    LLREF_LRECT* ll = NULL;

    if(rq==NULL || *rq==NULL){return;}

    // Empty lrect_queue
    lrect_queue_empty();

    time_interval = next_release_time();

    for(p=*rq;p!=NULL;p=p->next)
    {
        if(p->et==0){continue;}

        ll = (LLREF_LRECT*)malloc(sizeof(LLREF_LRECT));
        if(ll == NULL)
        {
            fprintf(stderr,"Error in malloc\n");
            return;
        }
        ll->local_remaining_execution_time = local_remaining_execution_time(p,time_interval);
        ll->tcb = p;
        ll->next = NULL;
        ll->next = lrect_queue;
        lrect_queue = ll;
    }
}

int task_exist(TCB** rq,TCB* tcb)
{
    TCB* p = NULL;
    if(rq==NULL||*rq==NULL||tcb==NULL){return 1;}

    for(p=*rq;p;p=p->next)
    {
        if(p==tcb){return 1;}
    }
    
    return 0;
}

void lrect_queue_rebuild(TCB** rq)
{
    LLREF_LRECT* ll1    = NULL;
    LLREF_LRECT* ll_tmp = NULL;
    LLREF_LRECT* ll_new = NULL;

    if(lrect_queue==NULL){return;}
    if(rq==NULL || *rq==NULL)
    {
        lrect_queue_empty();
        return;
    }

    for(ll_tmp=lrect_queue;ll_tmp;)
    {
        if(!task_exist(rq,ll_tmp->tcb))
        {
            ll1 = ll_tmp;
            ll_tmp = ll_tmp->next;
            free(ll1);
        }
        else
        {
            ll1 = ll_tmp;
            ll_tmp = ll_tmp->next;

            ll1->next = ll_new;
            ll_new = ll1;
        }
    }

    lrect_queue = ll_new;
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
    return EDF_insert_OK(t1,t2);
}

int LLREF_assign_history_check(TCB* tcb)
{
    if(tcb == NULL){return -1;}

    return assign_history[tcb->tid];
}

void print_lrect_queue()
{
    TCB* p=p_ready_queue;
    LLREF_LRECT* ll = lrect_queue;

    fprintf(stderr,"=====================ready queue==================\n");
    while(p)
    {
        fprintf(stderr,"%p\t%d\t%d\n",p,p->tid,p->et);
        p = p->next;
    }
    fprintf(stderr,"==================================================\n");
    fprintf(stderr,"=====================lrect queue====================\n");
    while(ll)
    {
        fprintf(stderr,"%d\t%f\t%u\n",ll->tcb->tid,ll->local_remaining_execution_time,ll->tcb->et);
        ll  = ll->next;
    }
    fprintf(stderr,"====================================================\n");
}

/*
  According to the paper, they difined the "Second Event",
  such some tasks' hit the bottom of T-L plan and some token hit the 
  that thing
*/
int LLREF_check_Second_Event()
{
    int i;
    double lrect = 0.0;
    LLREF_LRECT* ll;


    for(i=0;i<PROCESSOR_NUM;i++)
    {
        // Some processors are idleing, might because some task get complete.
        if(_kernel_runtsk[i]==NULL){return 1;}
    }

    for(ll=lrect_queue;ll;ll=ll->next)
    {
        lrect = ll->local_remaining_execution_time;
        //The Event B in second Event
        if(lrect<=(double)0)
        {
            //fprintf(stderr,"tid %d exausted\n",ll->tcb->tid);
            return 1;
        }
        //The Event C in second Event
        else if(((unsigned long long)lrect+1==time_interval&&lrect>(double)time_interval) || \
                 (unsigned long long)lrect  ==time_interval)
        {
            //fprintf(stderr,"tid %d hit the bound\n",ll->tcb->tid);
            return 1;
        }
    }
    return 0;
}

void LLREF_reduce_lrect()
{
    int i,changed;
    LLREF_LRECT* ll = NULL;

    for(ll=lrect_queue,changed=0;ll&&changed<PROCESSOR_NUM;ll=ll->next,changed++)
    {
        for(i=0;i<PROCESSOR_NUM;i++)
        {
            if(_kernel_runtsk[i]==NULL){continue;}

            if(_kernel_runtsk[i]==ll->tcb)
            {
                ll->local_remaining_execution_time-=1;
                break;
            }
        }   
    }
}

void LLREF_scheduling()
{    
    int i;
    int processor_id=-1;
    int assigned[PROCESSOR_NUM] = {0};
    int total_assigned = 0;
    TCB*         p  = NULL;
    TCB*         ap = NULL;
    LLREF_LRECT* ll = NULL;
    //fprintf(stderr,"ss\t");
    if(time_interval>0){time_interval--;}
    LLREF_reduce_lrect();
    if(time_interval<=0)
    {   
        //fprintf(stderr,"Event 1\n");
        LLREF_lrect_queue_build(&p_ready_queue);
    }
    else
    {
        // Check the Second Event has happened or not
        if(!LLREF_check_Second_Event()){return;}
        //fprintf(stderr,"Event 2\n");
        lrect_queue_rebuild(&p_ready_queue);
    }
    //lrect_queue is only for periodic tasks,so it might be NULL
    print_lrect_queue();
    LLREF_lrect_queue_sort(&lrect_queue); 
    //fprintf(stderr,"inLLrefat%llu\n",tick);

    for(i=0;i<PROCESSOR_NUM;i++){_kernel_runtsk[i] = NULL;}

    ll = lrect_queue;
    for(;ll!=NULL&&total_assigned<PROCESSOR_NUM;ll=ll->next,total_assigned++)
    {
        p = ll->tcb;
        processor_id = LLREF_assign_history_check(p);
        if(processor_id==-1 || assigned[processor_id])
        {
            // Search a available processor
            for(i=0;i<PROCESSOR_NUM;i++)
            {
                if(!assigned[i]){break;}
            }
            if(i<PROCESSOR_NUM)
            {
                _kernel_runtsk[i] = p;
                assigned[i] = 1;
            }
        }
        else
        {
            _kernel_runtsk[processor_id] = p;
            assigned[processor_id] = 1;
        }

        if(_kernel_runtsk_pre[processor_id] != _kernel_runtsk[processor_id])
        {
            _kernel_runtsk_pre[processor_id] = _kernel_runtsk[processor_id];
        }
    }

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(_kernel_runtsk==NULL)
        {
            _kernel_runtsk[i] = ap;
            if(ap!=NULL){ap = ap->next;}

            if ( _kernel_runtsk[i] != _kernel_runtsk_pre[i] ) 
            {
                _kernel_runtsk_pre[i] = _kernel_runtsk[i];
            } 
        }
    }
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        fprintf(stderr,"%d\t%p\n",i,_kernel_runtsk[i]);
    }
    //getchar();
}