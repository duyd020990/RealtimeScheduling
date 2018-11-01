#include "LLREF.h"

//#include <math.h>

#include "Schedule.h"
#include "signal.h"
#include "log/log.h"
#include "tool/tool.h"

#include "EDF.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define DEBUG


extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

extern unsigned long long period[MAX_TASKS];
extern unsigned long long tick;
extern unsigned wcet[MAX_TASKS];

extern int has_new_task;
extern int has_new_instance;
extern int has_task_finished;
//extern int has_task_missed;

extern unsigned long long overhead_dl;

int release_time[TICKS];
int assign_history[MAX_TASKS];

unsigned long long time_interval;
unsigned long long rest_time_interval;

int LLREF_involke;

#ifdef DEBUG
FILE* f_debug;
#endif

SCHEDULING_ALGORITHM LLREF_sa={
    .name                  = "LLREF",
    .scheduling_initialize = LLREF_scheduling_initialize,
    .scheduling_exit       = LLREF_scheduling_exit,
    .scheduling            = LLREF_scheduling,
    .insert_OK             = LLREF_insert_OK,
    .scheduling_update     = LLREF_scheduling_update,
    .reorganize_function   = LLREF_reorganize_function,
    .job_delete            = NULL
};

unsigned long long next_release_time()
{
    unsigned long long tick_tmp = 0;

    for(tick_tmp = tick+1;!release_time[tick_tmp];tick_tmp++)
    {
        overhead_dl += IADD + MEM;
    }

    overhead_dl += IADD;
    return tick_tmp - tick;
}
/*
  Here we will use the tick and array release_time
*/
double local_remaining_execution_time(unsigned r_wcet,unsigned long long part_time_interval,unsigned long long period,double rest_part_of_lrect)
{
    double lrect;

    overhead_dl += FMUL+FDIV;
    lrect = ((double)r_wcet*(rest_time_interval))/period;

#ifdef DEBUG
    log_c("%u\t%llu\t%llu\t%f\t%f\n",r_wcet,
                                          part_time_interval,
                                          period,
                                          lrect,
                                          rest_part_of_lrect);
#endif

    overhead_dl += FADD;
    return lrect+rest_part_of_lrect;
}

/*
  Sort the waiting queue according to the local remainning execution time.
*/

void LLREF_swap(TCB* t1,TCB* t2)
{
    TCB TCB_tmp;

    overhead_dl += COMP+COMP;
    if(t1==NULL || t2==NULL){return;}

    overhead_dl += MEM+IADD + MEM+IADD + MEM+IADD;
    memcpy(&TCB_tmp,t1,DIFF(TCB,next));
    memcpy(t1      ,t2,DIFF(TCB,next));
    memcpy(t2,&TCB_tmp,DIFF(TCB,next));
}

void lrect_sort(TCB** rq)
{
    TCB* p = NULL;
    TCB* q = NULL;
    LLREF_LRECT* ll1 = NULL;
    LLREF_LRECT* ll2 = NULL;

    if(rq==NULL || *rq==NULL){return;}

    p = *rq;
    for(;p!=NULL;p=p->next)
    {
        overhead_dl += IADD+COMP;
        for(q=p->next;q!=NULL;q=q->next)
        {
            overhead_dl += IADD+COMP;
            ll1 = (LLREF_LRECT*)(p->something_else);
            ll2 = (LLREF_LRECT*)(q->something_else);

            overhead_dl += IADD+COMP;
            if(ll1->local_remaining_execution_time < ll2->local_remaining_execution_time){LLREF_swap(p,q);}
        }
    }
}

LLREF_LRECT* lrect_node_init(TCB* tcb,unsigned long long rest_time_interval)
{
    LLREF_LRECT* ll = NULL;

    overhead_dl += COMP;
    if(tcb == NULL){return NULL;}

    overhead_dl = ASSIGN+MEM+COMP;
    ll = (LLREF_LRECT*)malloc(sizeof(LLREF_LRECT));
    if(ll == NULL)
    {
        log_once(NULL,"Err with malloc *llref_node_init*\n");
        return NULL;
    }

    overhead_dl += MEM; 
    ll->r_wcet                         = wcet[tcb->tid];
    ll->local_remaining_execution_time = local_remaining_execution_time(wcet[tcb->tid],rest_time_interval,period[tcb->tid],0.0);
    return ll;
}

#ifdef DEBUG
void print_lrect_queue()
{
    TCB* p=p_ready_queue;

    log_c("=============================ready queue=============================\n");
    log_c("addre\t\ttid\tret\tret\tlrect\tinterval period\n");
    while(p)
    {
        log_c("%p\t",p);
        if(p->something_else == NULL){p = p->next;continue;}
        log_c("%d\t%u\t%u\t%f\t%llu\t%llu\n",
                                              p->tid,
                                              p->et,
                                              ((LLREF_LRECT*)(p->something_else))->r_wcet,
                                              ((LLREF_LRECT*)(p->something_else))->local_remaining_execution_time,
                                              time_interval,
                                              period[p->tid]);
        p = p->next;
    }
    log_c("====================================================================\n");
}
#endif

void LLREF_reduce_lrect()
{
    int i;
    LLREF_LRECT* ll = NULL;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        overhead_dl += COMP+IADD;   //becuase of the for loop
        overhead_dl += COMP;      
        if(_kernel_runtsk[i]==NULL){continue;}

        overhead_dl += MEM+COMP;
        ll = (LLREF_LRECT*)_kernel_runtsk[i]->something_else;
        if(ll == NULL){log_once(NULL,"Should not be NULL, in LLREF reduce lrect\n");return;}

        overhead_dl += MEM+IADD+IADD;
        ll->r_wcet -= 1;
        ll->local_remaining_execution_time -= 1;
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

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){return 0;}
    
    overhead_dl += COMP;
    if(has_task_finished)
    {
        has_task_finished = 0;
#ifdef DEBUG
        log_c("has task finished,in LLREF_check_Second_Event\n");
#endif
        return 1;
    }

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if(_kernel_runtsk[i]==NULL){continue;}
        
        overhead_dl += MEM;
        ll = (LLREF_LRECT*)(_kernel_runtsk[i]->something_else);
        if(ll == NULL)
        {
            log_once(NULL,"Err with ll,in check second event\n");
            kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
        }
        
        overhead_dl += COMP;
        lrect = ll->local_remaining_execution_time;
        log_c("tid lrect exhausted %llu\t%d\t%.20f\n",tick,_kernel_runtsk[i]->tid,lrect);
        if(lrect<(double)0 || IS_EQUAL(lrect,(double)0))
        {
#ifdef DEBUG
            log_c("tid lrect exhausted %d\n",_kernel_runtsk[i]->tid);
#endif
            return 1;
        }
    }

    for(p=*rq;p;p=p->next)
    {
        overhead_dl += IADD+COMP;

        overhead_dl += COMP;
        ll = (LLREF_LRECT*)(p->something_else);
        if(ll == NULL)
        {
            log_once(NULL,"Err with ll,in check second event\n");
            kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
        }
        
        lrect = ll->local_remaining_execution_time;

        //The Event C in second Event
        overhead_dl += COMP;
        if((unsigned long long)lrect >= rest_time_interval)
        {
#ifdef DEBUG
            log_once(NULL,"tid %d hit the bound\n",p->tid);
#endif
            return 1;
        }
    }
    return 0;
}

int LLREF_assign_history_check(TCB* tcb)
{
    overhead_dl += COMP;
    if(tcb == NULL){return -1;}

    overhead_dl += MEM;
    return assign_history[tcb->tid];
}

void LLREF_scheduling_initialize()
{
    int i;
    time_interval      = 0;
    rest_time_interval = 0;
    LLREF_involke      = 0;
#ifdef DEBUG
    log_open("LLREF_debug.txt");
#endif
    for(i=0;i<MAX_TASKS;i++)
    {
        overhead_dl += IADD+COMP+MEM;
        assign_history[i] = -1;
    }   
}

void LLREF_reorganize_function(TCB** rq)
{
    TCB* p = NULL;
    LLREF_LRECT* ll  = NULL;

    overhead_dl += COMP+COMP;
    if(rq == NULL || *rq == NULL){return;}

    // Check the first event : new task released
    if(!release_time[tick]) 
    {
        //no new task released, check second event : boundry hitting event
        if(!LLREF_check_Second_Event(rq))
        {
            // no boundry hitting event
            return;
        }
#ifdef DEBUG
        log_c("Event 2 happened\n");
#endif
    }
    else
    {

#ifdef DEBUG
        log_c("Event 1 happened\n" );
#endif   
        
        rest_time_interval = time_interval = next_release_time();

#ifdef DEBUG
        log_c("time_interval %llu\n",time_interval);
#endif  
        
        for(p=*rq;p;p=p->next)
        {
            overhead_dl += IADD+COMP;
            overhead_dl += COMP;
            ll = (LLREF_LRECT*)(p->something_else);
            if(ll == NULL)
            {
                ll = lrect_node_init(p,rest_time_interval);
                p->something_else = ll;
            }
            else
            {
                ll->r_wcet = p->et;
                ll->local_remaining_execution_time = local_remaining_execution_time(p->wcet,time_interval,period[p->tid],ll->local_remaining_execution_time);
            }
        }
    }

#ifdef DEBUG
    log_c("At tick: %llu\n",tick);
    log_c("in reorganize\n");
    //getchar();
#endif

    // later we will call the scheduler
    LLREF_involke = 1;
    lrect_sort(rq);
}

int LLREF_insert_OK(TCB* t1,TCB* t2)
{
    overhead_dl += COMP;
    if(t1 == NULL){return 0;}
    else{return 1;};
}

void LLREF_scheduling_update()
{
    // Reduce the rest time interval and the rest 
    overhead_dl += IADD;
    rest_time_interval--;
    LLREF_reduce_lrect();
}

void LLREF_scheduling()
{    
    int  i;
    int  processor_id            = -1;
    int  assigned[PROCESSOR_NUM] = {0};
    TCB* p                       = NULL;
    TCB* ap                      = NULL;

    overhead_dl += COMP;
    if(p_ready_queue == NULL){return;}

    overhead_dl += COMP;
    if(!LLREF_involke){return;}
    LLREF_involke = 0;

#ifdef DEBUG
    log_c("in scheduling\n");
    print_lrect_queue();
#endif

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        overhead_dl += IADD+COMP;
        _kernel_runtsk[i] = NULL;
    }

    // Actually, at this moment, the p_ready_queue should be already sorteded by local_remaining_execution_time;

    for(p=p_ready_queue;p;p=p->next)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if(p->et <= 0){continue;}

        overhead_dl += COMP+COMP+MEM;
        processor_id = LLREF_assign_history_check(p);
        if(processor_id==-1 || assigned[processor_id])
        {
            // Search a available processor
            processor_id = -1;
            for(i=0;i<PROCESSOR_NUM;i++)
            {
                overhead_dl += IADD+COMP;
                overhead_dl += COMP;
                if(!assigned[i]){break;}
            }
            overhead_dl += COMP;
            if((0<=i) && (i<PROCESSOR_NUM)){processor_id = i;}
            else{break;}
        }
        assign_history[p->tid] = processor_id;
        _kernel_runtsk[processor_id] = p;
        assigned[processor_id] = 1;

        if(_kernel_runtsk_pre[processor_id] != _kernel_runtsk[processor_id])
        {
            _kernel_runtsk_pre[processor_id] = _kernel_runtsk[processor_id];
        }
    }

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if(_kernel_runtsk[i]==NULL)
        {
            overhead_dl += COMP;
            _kernel_runtsk[i] = ap;
            if(ap!=NULL){ap = ap->next;}

            overhead_dl += COMP;
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
        log_c("%d\t%d\n",i,_kernel_runtsk[i]->tid);
    }
#endif

}

void LLREF_scheduling_exit()
{
#ifdef DEBUG
    log_close();
#endif
}