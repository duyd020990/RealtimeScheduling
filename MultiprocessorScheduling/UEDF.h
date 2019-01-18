#ifndef UEDF_H
#define UEDF_H

#define UEDF

#include "Schedule.h"

typedef struct execution_phase
{
    TCB* tcb;
    unsigned et;
    struct execution_phase* next;
}EXECUTION_PHASE;

typedef struct
{
    int tid;
    unsigned long long req_time;
    unsigned long long a_dl;
    double ultilization[PROCESSOR_NUM];
    EXECUTION_PHASE ep[PROCESSOR_NUM];
}ASSIGNMENT_PHASE;

typedef struct tcb_cntnr
{
    TCB*              tcb;
    unsigned          et;
    struct tcb_cntnr* next;
}TCB_CNTNR;

void UEDF_scheduling_initialize();

void UEDF_scheduling_exit();

int UEDF_insert_OK(TCB* tcb1,TCB* tcb2);

void UEDF_reorganize_function(TCB** rq);

void UEDF_scheduling_update();

void UEDF_scheduling();

#endif