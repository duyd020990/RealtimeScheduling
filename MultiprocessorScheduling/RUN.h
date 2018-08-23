#ifndef RUN_H
#define RUN_H

#include "Schedule.h"

int RUN_insert_OK(TCB*,TCB*);

void RUN_reorganize_function(TCB**);

void RUN_schedule();


#endif