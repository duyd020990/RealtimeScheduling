#ifndef SCHEDULE_H
#define SCHEDULE_H
    

/* Overhead Definition */
#define FADD     4
#define FMUL     5
#define FDIV    15
#define IADD     1
#define ILOG     1
#define IMUL     2
#define COMP     1
#define ASSIGN   1
#define CEIL     1
#define FLOOR    1
#define MEM      1

#define PROCESSOR_NUM   4
#define UTIL_UPPERBOUND (double)PROCESSOR_NUM

/* General Definitions */
#define MAX_TASKS        100
#define MAX_INSTANCES  50000
#define TICKS         100000

#define  C0_cbs_Q_DIV  1

/* Definitions for Adaptive TBS */
unsigned int  C0;          /* 1st PET */
#define       Ci   1       /* Following PET */


/* Definitions for Virtual Release Advancing */
#define  VRA_BOUND 100000

/* Definitions for TB* / TB(n) */
#define  ITBS_BOUND  4096

/* Definitions for CBS */
unsigned int cbs_Q;         /* average for Exponential distribution used */
#define      cbs_T  (ceil ( (double)cbs_Q / (1.0 - p_util) ))


/* Macros */
#define max(a,b) ((a) > (b) ? (a) : (b))
#define max3(a,b,c) ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define min3(a,b,c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

#define DIFF(a,b) (unsigned long)(&(((a*)0)->b))-0

/***********************************************************************************************************/
/* TCB                                                                                                     */
/***********************************************************************************************************/
typedef struct tcb {
    int                tid;
    int                inst_no;
    unsigned long long req_tim;
    unsigned long long a_dl;
    unsigned           et;
    unsigned           wcet;
    unsigned           initial_et;
    unsigned           priority;
    void*              something_else;
    struct tcb         *next;
    struct tcb         *prev;
} TCB;

typedef struct 
{
    void* scheduling_initialize;    
    void* scheduling;
    void* insert_OK;
    void* reorganize_function;

}SCHEDULING_ALGORITHM;


/* Prototype */
void   insert_queue ( TCB **rq, TCB *entry, void* function);
void   insert_queue_RM ( TCB **rq, TCB *entry );
void   insert_queue_fifo ( TCB **rq, TCB *entry );
void   delete_queue(TCB** rq,TCB* tcb);
//void   delete_queue ( TCB **rq,int tid );
void   from_fifo_to_ap ( void );
void   run_simulation(char* s,SCHEDULING_ALGORITHM sa);
void   Initialize ( void );
void   Tick_inc ( );
TCB   *entry_set ( int );
void   Job_exit ( const char *, int ,int );
void   Overhead_Record ( void );

#endif