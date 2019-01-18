#include "UEDF.h"
#include "EDF.h"

#include "Schedule.h"

#include "signal.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <unistd.h>

#define DEBUG

extern int has_new_instance;
extern int has_new_task;
extern double job_util[MAX_TASKS];
extern unsigned long long period[MAX_TASKS];

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];

int assignment_history[MAX_TASKS];
int server_finished;

TCB_CNTNR* running_server[PROCESSOR_NUM];
TCB_CNTNR* execution_queue[PROCESSOR_NUM];

SCHEDULING_ALGORITHM UEDF_sa={
    .scheduling_initialize = UEDF_scheduling_initialize,
    .scheduling_exit      = UEDF_scheduling_exit,
    .scheduling           = UEDF_scheduling,
    .insert_OK            = UEDF_insert_OK,
    .reorganize_function  = UEDF_reorganize_function,
    .scheduling_update    = UEDF_scheduling_update
    .job_delete           = NULL
};

#ifdef DEBUG
void UEDF_execution_queue_print()
{

}

void UEDF_TCB_list_with_ul_print(TCB** rq)
{

}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * Formula for calculating reservation of task i on cpu j:
 * res_i_j(t1,t2)=(t2-t1)*u_i_j(t1)
 * t1 and t2 indicate a time interval
 * u_i_j: ultilization of task i on processor j
 * */
unsigned res_calcualte(ASSIGNMENT_PHASE* ap,int cpu_id,unsigned long long t1,unsigned long long t2)
{
    if(ap == NULL){return 0;}
    
    if(cpu_id<0 || cpu_id>=PROCESSOR_NUM){return 0;}
    
    if(t1 >= t2){return 0;}
    
    return ceil((t2-t1)*(ap->ultilization[cpu_id]));
}

/*
 * Accordind to paper, the formula should be:
 * bdg_i_j(t1,t2) = al_i_j(t1)+res_i_j(d_i(t1),t2)
 * i: the id of task
 * j: cpu_id
 * t1 and t2 indicated a time interval
 * al_i_j: the alloment of task i on cpu j
 * res_i_j: the future execution time of task i assigned on cpu j. 
 * Here,I didn't use i to indicate the task, I will use TCB
 * */
unsigned bdg_calculate(ASSIGNMENT_PHASE* ap,int cpu_id,unsigned long long t1,unsigned long long t2)
{
    unsigned et = 0;
    unsigned bdg = 0;
    
    EXECUTION_PHASE* ep = NULL;
    
    if(ap == NULL){return 0;} 

    if(cpu_id<0 || cpu_id>=PROCESSOR_NUM){return 0;}
    
    ep = &(ap->ep[cpu_id]);
    
    return ep->et + res_calcualte(ap,cpu_id,ap->a_dl,t2);
}

/*
 * Formula for calculatting al_max_i_j:
 * al_max_i_j=(d_i(t)-t)-SUM(bdg_x_j(t,d_i(t))|dl of task x is earlier than i's)-SUM(al_i_y(t)|index of y is smaller than j)
 * */
unsigned al_max_i_j_calculate(ASSIGNEMNT_PHASE* ap_list,ASSIGNMENT_PAHSE* ap,int cpu_id,unsigned long long t)
{
    int i;
    unsigned bdg          = 0;
    unsigned al_sum       = 0;
    unsigned long long dl = 0;
    ASSIGNMENT_PHASE* AP_tmp = NULL;
    
    if(ap_list==NULL || ap==NULL){return 0;}
    
    if(cpu_id<0 || cpu_id>=PROCESSOR_NUM){return 0;}
    
    dl = ap->a_dl;
    
    for(AP_tmp=ap_list;AP_tmp;AP_tmp=AT_tmp->next)
    {
        if(AP_tmp == ap){break;}
        // i.a_dl is in [tick,ap->dl]
        if(AP_tmp->a_dl>=tick && AP_tmp->a_dl<dl)
        {
            bdg += bdg_calculate(AP_tmp,cpu_id,tick,dl);
        }
    }
    
    for(i=0;i<cpu_id;i++)
    {
        al_sum += (ap->ep)[i].et;
    }
    
    return (dl-tick)-bdg-al_sum;
}

/*
 * t here should be tick,just pass the tick as the last argument  
 * */
unsigned al_i_j_calculate(TCB* tcb,ASSIGNMENT_PHASE* ap,int cpu_id,unsigned long long t)
{
    int i;
    unsigned al_max        = 0;
    unsigned long long ret = 0;
    
    if(tcb==NULL || ap==NULL){return 0;}
    
    if(cpu_id<0 || cpu_id>=PROCESSOR_NUM){return 0;}
    
    ret = tcb->et;
    
    for(i=0;i<cpu_id;i++){ret -= ap->ep[i].et;}
    
    al_max = al_max_i_j_calculate(ap_list,ap,cpu_id,t)

    if(ret<al_max)
    {
        return ret;
    }
    return al_max;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////


// I think I should delete this function and implement a new one.
int TCB_to_TCB_CNTNRs(TCB_CNTNR* tc[],TCB* tcb)
{
    int i;
    TCB_CNTNR* tc_tmp = NULL;
    PROCESS_BLOCK* pb = NULL;

    if(tcb == NULL){return 0;}

    pb = (PROCESS_BLOCK*)(tcb->something_else);
    if(pb == NULL){fprintf(stderr,"pb is NULL,in TCB_to_TCB_CNTNR.\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        tc_tmp = (TCB_CNTNR*)malloc(sizeof(TCB_CNTNR));
        tc_tmp->tcb  = tcb;
        tc_tmp->et   = ceil((pb->ultilization[i])*period[tcb->tid]);
        tc_tmp->next = NULL;
        tc[i]        = tc_tmp;
    }

    return 1;
}

void UEDF_execution_queue_insert(TCB_CNTNR* tc,int processor_id)
{
    TCB_CNTNR** tc_pointer = NULL;

    if(tc==NULL || processor_id<0 || processor_id>=PROCESSOR_NUM){return;}

    for(tc_pointer=&(execution_queue[processor_id]);*tc_pointer;tc_pointer=&((*tc_pointer)->next));
    *tc_pointer = tc;
}

void UEDF_execution_queue_build(TCB** rq)
{
    int i;
    TCB* tcb = NULL;
    TCB_CNTNR* tc[PROCESSOR_NUM] = {NULL};

    if(rq==NULL || *rq==NULL){return;}

    for(tcb=*rq;tcb;tcb=tcb->next)
    {
        if(TCB_to_TCB_CNTNRs(tc,tcb))
        {
            for(i=0;i<PROCESSOR_NUM;i++)
            {
                UEDF_execution_queue_insert(tc[i],i);
            }
        }
    }
}

/*
    This is how EDF-D select task;
*/
TCB_CNTNR* EDFD_execution_queue_select(int processor_id)
{
    int i;
    int running;
    TCB_CNTNR* tc = NULL;

    if(processor_id<0 || processor_id>=PROCESSOR_NUM){return NULL;}

    for(tc=execution_queue[processor_id];tc;tc=tc->next)
    {
        running = 0;
        for(i=0;i<PROCESSOR_NUM;i++)
        {
            if(tc->tcb == (running_server[i]->tcb)){running = 1;}
        }

        if(!running){return tc;}
    }

    return NULL;
}

TCB_CNTNR* UEDF_execution_queue_remove(TCB_CNTNR* tc,int processor_id)
{
    TCB_CNTNR** tc_pointer = NULL;

    if(tc == NULL){return NULL;}

    if(processor_id<0 || processor_id>=PROCESSOR_NUM){return NULL;}

    for(tc_pointer=&(execution_queue[processor_id]);*tc_pointer;)
    {
        if(*tc_pointer == tc)
        {
            *tc_pointer = (tc->next);
            break;
        }
        else{tc_pointer = &((*tc_pointer)->next);}
    }

    return tc;
}

void UEDF_execution_queue_destory()
{    
    int i;
    TCB_CNTNR*  tc         = NULL;
    TCB_CNTNR** tc_pointer = NULL;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        for(tc_pointer=&(execution_queue[i]);*tc_pointer;)
        {
            tc = *tc_pointer;
            tc_pointer = &((*tc_pointer)->next);
            free(tc);
        }
        execution_queue[i] = NULL;
    }
}

int running_server_update()
{
    int i;
    int zero_cnt = 0;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(running_server[i] != NULL)
        {
            (running_server[i]->et)--;
            if(running_server[i]->et == 0)
            {
                UEDF_execution_queue_remove(running_server[i],i);
                running_server[i] = NULL;
                zero_cnt++;
            }
        }
    }

    return zero_cnt;
}

void ultilization_assign(TCB** rq,TCB* tcb)
{
    int i;
    TCB*           tcb_tmp = NULL;
    PROCESS_BLOCK* pb      = NULL;
    double total_ultilization1 = 0.0;
    double total_ultilization2 = 0.0;
    double tmp_ultilization1 = 0.0;
    double tmp_ultilization2 = 0.0;

    if(rq==NULL || *rq==NULL || tcb==NULL){return;}

    for(tcb_tmp=*rq;(tcb_tmp) && (tcb_tmp!=tcb);tcb_tmp=tcb_tmp->next)
    {
        total_ultilization1 += job_util[tcb->tid];
    }
    total_ultilization2 = total_ultilization1+job_util[tcb->tid];

    if(tcb->something_else == NULL)
    {
        pb = (PROCESS_BLOCK*)malloc(sizeof(PROCESS_BLOCK));
        tcb->something_else = (void*)pb;
    }
    else
    {pb = (PROCESS_BLOCK*)(tcb->something_else);}

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        tmp_ultilization2 = total_ultilization2 - (double)i;
        tmp_ultilization1 = total_ultilization1 - (double)i;

        if(tmp_ultilization2 > (double)1){tmp_ultilization2 = (double)1;}
        else if(tmp_ultilization2 < (double)0){tmp_ultilization2 = (double)0;}
        if(tmp_ultilization1 > (double)1){tmp_ultilization1 = (double)1;}
        else if(tmp_ultilization1 < (double)0){tmp_ultilization1 = (double)0;}

        pb->ultilization[i] = tmp_ultilization2 - tmp_ultilization1;
    }
}

void UEDF_scheduling_initialize()
{
    int i;
    
    for(i=0;i<MAX_TASKS;i++){assignment_history[i] = -1;}

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        running_server[i]  = \
        execution_queue[i] = NULL;
    }
}

void UEDF_scheduling_exit()
{
    UEDF_execution_queue_destory();
}

int UEDF_insert_OK(TCB* tcb1,TCB* tcb2)
{
    return EDF_insert_OK(tcb1,tcb2);
}

void UEDF_reorganize_function(TCB** rq)
{
    TCB* tcb = NULL;

    if(rq==NULL || *rq==NULL){return;}

    if(!has_new_instance){return;}

    UEDF_execution_queue_destory();

    for(tcb=*rq;tcb;tcb=tcb->next)
    {
        ultilization_assign(rq,tcb);
    }

    UEDF_execution_queue_build(rq);
}

void UEDF_scheduling_update()
{
    if(running_server_update()){server_finished = 1;}
}

void UEDF_scheduling()
{
    int i;
    TCB*       tcb = NULL;
    TCB_CNTNR* tc  = NULL;

    if(!has_new_instance || !server_finished){return;}

    if(has_new_instance)
    {
        has_new_instance = 0;

        for(i=0;i<PROCESSOR_NUM;i++)
        {
            _kernel_runtsk[i] = NULL;
            running_server[i] = NULL;
        }

        for(i=0;i<PROCESSOR_NUM;i++)
        {
            tc = EDFD_execution_queue_select(i);
            if(tc == NULL)
            {continue;}

            tcb = tc->tcb;
            if(tcb == NULL){fprintf(stderr,"tcb is NULL,in UEDF_scheduling.\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}
            
            _kernel_runtsk[i] = tcb;
        }

    }
    else if(server_finished)
    {
        server_finished = 0;

        for(i=0;i<PROCESSOR_NUM;i++)
        {
            if(running_server[i] == NULL)
            {
                tc = EDFD_execution_queue_select(i);
                if(tc == NULL){fprintf(stderr,"tc is NULL,in UEDF_scheduling.\n");continue;}

                tcb = tc->tcb;
                if(tcb == NULL){fprintf(stderr,"tcb is NULL,in UEDF_scheduling.\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

                _kernel_runtsk[i] = tcb;
            }
        }
    }

#ifdef DEBUG
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(_kernel_runtsk[i] == NULL){continue;}
        fprintf(stderr,"pid:%d\t%p\t%d\n",i,_kernel_runtsk[i],_kernel_runtsk[i]->tid);
    }
    getchar();
#endif
}