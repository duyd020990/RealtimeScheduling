#ifndef EDF_H
#define EDF_H

#define EDF

#include "Schedule.h"

void EDF_scheduling_initialize();

int EDF_insert_OK(TCB* t1,TCB* t2);

void EDF_reorganize_function(TCB** rq);

void EDF_Scheduling();


#endif