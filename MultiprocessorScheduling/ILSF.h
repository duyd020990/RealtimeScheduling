#ifndef ILSF_H
#define ILSF_H

#define ILSF

#include "Schedule.h"

#define THRESHOLD 2
#define CALCULATE_THRESHOLD // this macro for calculating the dynamic threshold

void ILSF_scheduling_initialize();

void ILSF_Scheduling();

int ILSF_insert_OK(TCB*,TCB*);

void ILSF_reorganize_function(TCB**);

#endif