#ifndef DATAGENETOR_H
#define DATAGENETOR_H

#define MAX_TASKS        100
#define MAX_INSTANCES  50000
#define TICKS         100000

#define CAP              4        // CAPacity of the system (the number of processor)
#define MAX_AP_SET       30
#define MAX_AP_TSK       4
#define INIT_AP_ID       100
#define MAX_OBSERVATION  100000
#define MAX_TSK          100      // maximum number of tasks
#define MAX_ITL          7        // maximum of intervals
#define MAX_SIZE         10       // number of task set/case
#define UTIL_LOWER_BOUND 0.1
#define UTIL_UPPER_BOUND 0.7

#define UNIDIS(l,r,h)   (h-1>0)?(r%(h-1))+l:1

#define	PERCENT_70  0
#define PERCENT_75  1
#define PERCENT_80  2
#define PERCENT_85  3
#define PERCENT_90  4
#define PERCENT_95  5
#define PERCENT_100 6

typedef struct
{
    int cap;
    int ulti;
}GEN_ARG;

void* generator(void*);

#endif