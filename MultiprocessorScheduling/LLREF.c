#include "LLREF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Schedule.h"
#include "EDF.h"

#define DEBUG


extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

extern unsigned long long period[MAX_TASKS];
extern unsigned long long tick;
extern unsigned wcet[MAX_TASKS];

int release_time[TICKS];
int assign_history[MAX_TASKS];

unsigned long long time_interval;
unsigned long long rest_time_interval;

int LLREF_involke;


// This running queue for those Multiprocessor Real-time Scheduling,
// Since the migration only occurse in Multiproccessor platform
TCB *running_queue[PROCESSOR_NUM]; // A running queue for each processor

SCHEDULING_ALGORITHM LLREF_sa={
    .scheduling_initialize = LLREF_scheduling_initialize,
    .scheduling            = LLREF_scheduling,
    .insert_OK             = LLREF_insert_OK,
    .reorganize_function   = LLREF_reorganize_function
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
double local_remaining_execution_time(unsigned r_wcet,unsigned long long part_time_interval,unsigned long long period)
{
    double lrect;
    lrect = ((double)r_wcet*(rest_time_interval))/period; 
#ifdef DEBUG
    fprintf(stderr,"%u\t%llu\t%llu\t%f\n",r_wcet,
                                          part_time_interval,
                                          period,
                                          lrect);
#endif
    return lrect;
}

/*
  Sort the waiting queue according to the local remainning execution time.
*/

void LLREF_swap(TCB* t1,TCB* t2)
{
    TCB TCB_tmp;

    if(t1==NULL || t2==NULL){return;}

    memcpy(&TCB_tmp,t1,DIFF(TCB,next));
    memcpy(t1      ,t2,DIFF(TCB,next));
    memcpy(t2,&TCB_tmp,DIFF(TCB,next));
}

LLREF_LRECT* lrect_node_init(TCB* tcb,unsigned long long rest_time_interval)
{
    LLREF_LRECT* ll = NULL;

    if(tcb == NULL){return NULL;}

    ll = (LLREF_LRECT*)malloc(sizeof(LLREF_LRECT));
    if(ll == NULL)
    {
        fprintf(stderr,"Err with malloc *llref_node_init*\n");
        return NULL;
    }

    ll->r_wcet                         = wcet[tcb->tid];
    ll->local_remaining_execution_time = local_remaining_execution_time(wcet[tcb->tid],rest_time_interval,period[tcb->tid]);

    return ll;
}

void print_lrect_queue()
{
    TCB* p=p_ready_queue;

    fprintf(stderr,"=====================ready queue==================\n");
    while(p)
    {
        fprintf(stderr,"%p\t",p);
        if(p->something_else==NULL){p = p->next;continue;}
        fprintf(stderr,"%d\t%u\t%u\t%f\t%llu\t%u\n",
                                              p->tid,
                                              p->et,
                                              ((LLREF_LRECT*)(p->something_else))->r_wcet,
                                              ((LLREF_LRECT*)(p->something_else))->local_remaining_execution_time,
                                              time_interval,
                                              wcet[p->tid]);
        p = p->next;
    }
    fprintf(stderr,"==================================================\n");
}

void LLREF_reduce_lrect()
{
    int i;
    LLREF_LRECT* ll = NULL;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(_kernel_runtsk[i]==NULL){continue;}

        ll = (LLREF_LRECT*)_kernel_runtsk[i]->something_else;
        if(ll == NULL){fprintf(stderr,"Should not be NULL, in LLREF reduce lrect\n");return;}

        ll->r_wcet-=1;
        ll->local_remaining_execution_time-=1;
    }     
}

/*
  According to the paper, they difined the "Second Event",
  such some tasks' hit the bottom of T-L plan and some token hit the 
  that thing
*/
int LLREF_check_Second_Event(TCB** rq)
{
    int          i     = 0;
    TCB*         p     = NULL;
    double       lrect = 0.0;
    LLREF_LRECT* ll    = NULL;

    if(rq==NULL || *rq==NULL){return 0;}

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        // Some processors are idleing, might because some task get complete.
        if(_kernel_runtsk[i]==NULL){return 1;}
    }

    for(p=*rq;p;p=p->next)
    {
        ll = (LLREF_LRECT*)(p->something_else);
        if(ll == NULL){fprintf(stderr,"Err with ll,in check second event\n");}
        
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

int LLREF_assign_history_check(TCB* tcb)
{
    if(tcb == NULL){return -1;}

    return assign_history[tcb->tid];
}

void LLREF_scheduling_initialize()
{
    int i;
    time_interval      = 0;
    rest_time_interval = 0;
    LLREF_involke      = 0;

    for(i=0;i<MAX_TASKS;i++)
    {
        assign_history[i] = -1;
    }   
}

void LLREF_reorganize_function(TCB** rq)
{
    TCB* p = NULL;
    TCB* q = NULL;
    LLREF_LRECT* ll  = NULL;
    LLREF_LRECT* ll1 = NULL;
    LLREF_LRECT* ll2 = NULL;

    if(rq == NULL || *rq == NULL){return;}

    // Reduce the rest time interval and the rest 
    rest_time_interval--;
    LLREF_reduce_lrect();

    // Check the first event : new task released
    if(!release_time[tick]) 
    {
        //no new task released, check second event : boundry hitting event
        if(!LLREF_check_Second_Event(rq))
        {
            // no boundry hitting ecent
            return;
        }
#ifdef DEBUG
        fprintf(stderr,"Event 2 happened\n");
#endif
    }
    else
    {

#ifdef DEBUG
        fprintf(stderr, "Event 1 happened\n" );
#endif   
        
        rest_time_interval = time_interval = next_release_time();

#ifdef DEBUG
        fprintf(stderr,"time_interval %llu\n",time_interval);
#endif  
        
        for(p=*rq;p;p=p->next)
        {
            ll = (LLREF_LRECT*)(p->something_else);
            if(ll == NULL)
            {
                ll = lrect_node_init(p,rest_time_interval);
                p->something_else = ll;
            }
            else
            {
                ll->local_remaining_execution_time = local_remaining_execution_time(ll->r_wcet,rest_time_interval,period[p->tid]);
            }
        }
    }
#ifdef DEBUG
    fprintf(stderr,"At tick: %llu\n",tick);
    fprintf(stderr,"in reorganize\n");
#endif

    // later we will call the scheduler
    LLREF_involke = 1;

    p = *rq;
    for(;p!=NULL;p=p->next)
    {
        for(q=p->next;q!=NULL;q=q->next)
        {
            ll1 = (LLREF_LRECT*)(p->something_else);
            ll2 = (LLREF_LRECT*)(q->something_else);

            if(ll1->local_remaining_execution_time<ll2->local_remaining_execution_time){LLREF_swap(p,q);}
        }
    }
}

int LLREF_insert_OK(TCB* t1,TCB* t2)
{
    if(t1 == NULL){return 0;}
    else{return 1;};
}

void LLREF_scheduling()
{    
    int  i;
    int  processor_id            = -1;
    int  assigned[PROCESSOR_NUM] = {0};
    int  total_assigned          = 0;
    TCB* p                       = NULL;
    TCB* ap                      = NULL;
    //fprintf(stderr,"ss\t");

    if(p_ready_queue == NULL){return;}

    if(!LLREF_involke){return;}
    LLREF_involke = 0;

#ifdef DEBUG
    fprintf(stderr,"in scheduling\n");
    print_lrect_queue();
#endif

    for(i=0;i<PROCESSOR_NUM;i++){_kernel_runtsk[i] = NULL;}

    // Actually, at this moment, the p_ready_queue should be already soreded by local_remaining_execution_time;

    for(p=p_ready_queue;p&&total_assigned<PROCESSOR_NUM;p=p->next,total_assigned++)
    {
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
    
#ifdef DEBUG
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(_kernel_runtsk[i]==NULL){continue;}
        fprintf(stderr,"%d\t%d\n",i,_kernel_runtsk[i] -> tid);
    }
#endif

}