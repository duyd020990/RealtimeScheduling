#ifndef LLREF_H
#define LLREF_H

#define LLREF

#include "Schedule.h"

typedef struct llref_lrect
{
    double local_remaining_execution_time;
    unsigned r_wcet;
}LLREF_LRECT;

void LLREF_scheduling_initialize();

void LLREF_scheduling();

int LLREF_insert_OK(TCB*,TCB*);

void LLREF_reorganize_function(TCB** rq);

void LLREF_scheduling_exit();

#endif