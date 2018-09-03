#include "EDF.h"

#include <stdio.h>
#include "Schedule.h"

#include "LSF.h"

extern TCB* _kernel_runtsk;
extern TCB* _kernel_runtsk_pre;
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

/* Variables for overheads estimation */
extern unsigned long long overhead_dl;
extern unsigned long long overhead_dl_max;
extern unsigned long long overhead_dl_total;
extern unsigned long long overhead_alpha;
extern unsigned long long overhead_alpha_max;
extern unsigned long long overhead_alpha_total;


SCHEDULING_ALGORITHM EDF_sa={
    .scheduling_initialize = EDF_scheduling_initialize,
    .scheduling            = EDF_Scheduling,
    .insert_OK             = EDF_insert_OK,
    .reorganize_function   = EDF_reorganize_function,
};

void EDF_scheduling_initialize()
{
    return;
}

int EDF_insert_OK(TCB* t1,TCB* t2)
{
    overhead_dl += COMP;
    if(t1==NULL)
    {
        return 0;
    }
    if(t2==NULL)
    {
        return 1;
    }
    if((t1->a_dl) < (t2->a_dl))
    {
        return 1;
    }
    return 0;
}

void EDF_reorganize_function(TCB** rq)
{
    return;
}

void EDF_Scheduling()
{
    fprintf(stderr,"EDF\n");
    
    LSF_Scheduling();
}