#ifndef LLREF_H
#define LLREF_H

#include "Schedule.h"

typedef struct llref_lrect
{
    unsigned long long local_remaining_execution_time;
    TCB* tcb;
    struct llref_lrect* next;
}LLREF_LRECT;

void LLREF_scheduling();

int LLREF_insert_OK(TCB*,TCB*);

void LLREF_reorganize_function(TCB** rq);

#endif