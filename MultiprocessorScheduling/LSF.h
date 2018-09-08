#ifndef LSF_H
#define LSF_H

#include "Schedule.h"

void LSF_scheduling_initialize();
void LSF_scheduling_exit();

int LSF_insert_OK(TCB* t1,TCB* t2);

void LSF_reorganize_function(TCB** rq);

void LSF_Scheduling();


#endif