/*
    Designed by myself, ultilization 70%
    Run following steps for everytime new job released.
    1.Sort tasks by using their deadline(EDF)
    2.Assign tasks in horizental
      (1) Define start_time and initialize it as current tick.(The start_time for record the current task assigned to current cpu will start run since start_time).
      (2) Allocate the task to processor horizentally, and update the start_time 
          a The task could assigned to current and without any deadline miss ==> update start_time and continue assign next task.
          b The task could assigned to current but there is deadline miss    ==> trunc this task,the execution_time_on_p = tcb->deadline - start_time.
                                                                                 execution_time_on_p+1 = tcb->et - execution_time_on_p,update start_time and start assign tasks on p+1
      (3)Scheduling tasks according the allocation.
*/

#include "HEDF.h"

#include "Schedule.h"
#include "signal/signal.h"
#include "log/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int running_TC_finished;
TCB_CNTNR* TC_new_job_list;
CPU_ASSIGNMENT cs_array[PROCESSOR_NUM];
TCB_CNTNR** tc_running[PROCESSOR_NUM];

extern int has_new_instance;
extern int has_new_task;
extern unsigned long long tick;
extern TCB* p_ready_queue;
extern TCB* _kernel_runtsk[PROCESSOR_NUM];

SCHEDULING_ALGORITHM HEDF_sa={
    .name                  = "HEDF",
    .scheduling_initialize = HEDF_scheduling_init,
    .scheduling_exit       = HEDF_scheduling_exit,
    .scheduling            = HEDF_scheduling,
    .insert_OK             = HEDF_insert_OK,
    .scheduling_update     = HEDF_scheduling_update,
    .reorganize_function   = HEDF_reorganize_function,
    .job_delete            = NULL,
};

/*******************************For debugging****************************/
void cs_array_queue_print()
{
    int          i = 0;
    TCB*       tcb = NULL;
    TCB_CNTNR* tc  = NULL;

    for(i=0 ; i < PROCESSOR_NUM ; i++)
    {
        if(NULL == cs_array[i].head){continue;}
        
        log_c("processor: %d \n",i);
        for(tc=cs_array[i].head;tc;tc=tc->next)
        {
            tcb = tc->tcb;
            if(NULL == tcb){log_c("tcb is NULL,in TC_ready_queue_print\n");continue;}
            log_c("\ttid:%d,st:%lu,et:%lu\n",tc->tcb->tid,
                                             tc->start_time,
                                             tc->et);
        }
    }
}

void TC_ready_queue_print(TCB_CNTNR** tc_list)
{
    int         i = 0;
    TCB_CNTNR* tc = NULL;

    if(tc_list == NULL){log_c("tc_list is NULL, in TC_ready_queue\n");return;}

    for(tc=*tc_list,i=0;tc;tc=tc->next,i++)
    {
        log_c("%d,tid:%d,st:%lu,et:%lu,a_dl:%lu\n",
                                          i,
                                          tc->tcb->tid,
                                          tc->start_time,
                                          tc->et);
    }
}

/************************************************************************/

void TCB_CNTNR_fill(TCB_CNTNR* tc,unsigned long start_time,unsigned long et,TCB* tcb)
{
    if(NULL == tc){return;}

    if(NULL == tcb){return;}

    tc->start_time = start_time;
    tc->et         = et;
    tc->tcb        = tcb;
}


TCB_CNTNR* TCB_CNTNR_alloc(unsigned long start_time,unsigned long et,TCB* tcb)
{
    TCB_CNTNR* tc = NULL;

    if(NULL == tcb){return NULL;}
    
    tc = (TCB_CNTNR*)malloc(sizeof(TCB_CNTNR));
    if(NULL == tc)
    {
        // Log out the message and halt this program,
        // No need to continue.
        log_once(NULL,"Error with malloc,in TCB_CNTNR_alloc\n");
        kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
    }
 
    TCB_CNTNR_fill(tc,start_time,et,tcb);

    return tc;
}

int TCB_CNTNR_insert(TCB_CNTNR** tc_list,TCB_CNTNR* tc)
{
    TCB_CNTNR** tc_pp = NULL;

    if(NULL == tc){return 0;}

    if(NULL == tc_list){return 0;}

    for(tc_pp = tc_list ; *tc_pp ; tc_pp = &((*tc_pp)->next));

    tc->next = *tc_pp;
    *tc_pp   = tc;

    return 1;
}

TCB_CNTNR* TCB_CNTNR_retrieve(TCB_CNTNR** tc_list)
{
    TCB_CNTNR* tc_p = NULL;

    if(NULL == tc_list){return NULL;}

    if(NULL == *tc_list){return NULL;}

    tc_p       = *tc_list;

    *tc_list   = tc_p->next;

    tc_p->next = NULL;

    return tc_p;
}

void cs_array_destory()
{
    int      cpu_id = 0;

    TCB_CNTNR* tc_p = NULL;

    for(cpu_id = 0 ; cpu_id<PROCESSOR_NUM ; cpu_id++)
    {
        while(NULL != (tc_p = TCB_CNTNR_retrieve(&(cs_array[cpu_id].head))))
        {
            free(tc_p);
        }
    }
}

void HEDF_scheduling_init()
{
    int i;
    
    for(i = 0 ; i < PROCESSOR_NUM ; i++)
    {
        cs_array[i].head   = \
        cs_array[i].tail   = NULL;
        tc_running[i]      = NULL;
    }

    TC_new_job_list = NULL; 

    running_TC_finished = 0;

    log_open("HEDF_debug.txt");
}

void HEDF_scheduling_update()
{
    int i;

    TCB_CNTNR* tc = NULL;

    for(i = 0 ; i < PROCESSOR_NUM ; i++)
    {
        if(NULL == cs_array[i].head)
        {
            continue;
        }

        (cs_array[i].head->et)--;
        if(0 == (cs_array[i].head->et))
        {
            // Set flag, later call the schedule function
            running_TC_finished |= 1;

            // Unchain the exhausted tc from its list, and release the space.
            tc = cs_array[i].head;
            cs_array[i].head = tc->next;

            free(tc);
        }

    }
}

int HEDF_insert_OK(TCB* t1,TCB* t2)
{
    if(NULL == t1){return 0;}

    if(NULL == t2){return 1;}

    if(t1->a_dl < t2->a_dl){return 1;}

    return 0;
}

void HEDF_reorganize_function(TCB** rq)
{
    int                cpu_id            = -1;
    unsigned long      et                = 0;
    unsigned long      et1               = 0;
    unsigned long      et2               = 0; 
    unsigned long      start_time        = tick;
    TCB*               tcb_p             = NULL;
    TCB_CNTNR*         tc_p1             = NULL;
    TCB_CNTNR*         tc_p2             = NULL;

    if(!has_new_task && !has_new_instance)
    {
        return;
    }

    if((NULL == rq) || (NULL == *rq))
    {
        return;
    }

    log_c("In HEDF_reorganize_function, At tick : %lu\n",tick);

    //Enumerate the sorted tcb list,and allocate them to CPU
    TCB_list_print(*rq);

    // Remove all those allocation
    cs_array_destory();

    cpu_id = 0;
    start_time = tick;
    for(tcb_p = *rq ; tcb_p && (cpu_id < PROCESSOR_NUM); tcb_p = tcb_p->next)
    {
        if(0 == tcb_p->et){continue;}

        // Some slack time on core_p
        if(start_time < (tcb_p->a_dl))
        {
            //Here we have to split task first part to core_p second part core_p+1
            et = tcb_p->et;
            if((start_time + et) > (tcb_p->a_dl))                // Bigger case
            {
                et1 = (tcb_p->a_dl) - start_time;
                et2 = et - et1;

                tc_p1 = TCB_CNTNR_alloc(start_time,et1,tcb_p);

                tc_p2 = TCB_CNTNR_alloc(tick      ,et2,tcb_p);

                // Link tc_p1 into ready queue of core_p
                TCB_CNTNR_insert(&(cs_array[cpu_id].head),tc_p1);

                // Link tc_p2 into ready queue of core_p+1
                // Reset start_time (not to tick)
                // cpu_id++;
                if((cpu_id+1) >= PROCESSOR_NUM)
                {
                    cs_array_queue_print();
                    log_c("i+1 > PROCESSOR_NUM, HEDF_reorganize_function\n");
                    kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
                }

                TCB_CNTNR_insert(&(cs_array[cpu_id+1].head),tc_p2);

                //Calculate the start_time and cpu_id
                start_time = tick + et2;
                cpu_id++;
            }
            else if((start_time + et) == (tcb_p->a_dl))     // Smaller and "equal"(important) case
            {
                // Allocate tc_p1
                // Link tc_p1 into ready queue of core_p
                tc_p1 = TCB_CNTNR_alloc(start_time,et,tcb_p);

                TCB_CNTNR_insert(&(cs_array[cpu_id].head),tc_p1);

                start_time = tick;
                cpu_id++;
            }
            else
            {
                tc_p1 = TCB_CNTNR_alloc(start_time,et,tcb_p);

                TCB_CNTNR_insert(&(cs_array[cpu_id].head),tc_p1);

                start_time += et;
            }
        }
        else if(start_time == (tcb_p->a_dl))
        {
            start_time = tick;
            cpu_id++;
        }
        else
        {
            log_c("start_time > tcb_p->a_dl,in HEDF_reorganize_function\n");
            kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
        }
    }

    // Check the allocation
    cs_array_queue_print();
    //getchar();
}

void HEDF_scheduling()
{
    int i = 0;

    if(!running_TC_finished && !has_new_task && !has_new_instance){return;}
    
    has_new_instance    = \
    has_new_task        = \
    running_TC_finished = 0;

    log_c("In scheduling,at tick : %lu\n",tick);

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(NULL == cs_array[i].head)
        {
            
            _kernel_runtsk[i] = NULL;
            continue;
        }
        _kernel_runtsk[i] = (cs_array[i].head)->tcb;
    }

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(NULL == _kernel_runtsk[i])
        {
            continue;
        }
        log_c("CPU:%d,%d\n",i,_kernel_runtsk[i]->tid);
    }
}

void HEDF_scheduling_exit()
{
    log_close();
}
