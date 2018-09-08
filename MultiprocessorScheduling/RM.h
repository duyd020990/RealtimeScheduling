#ifndef RM_H
#define RM_H

#define RM

#include "Schedule.h"

void RM_scheduling_initialize();
void RM_scheduling_exit();

int RM_insert_OK();

void RM_reorganize_function(TCB** rq);

void RM_Scheduling();


#endif