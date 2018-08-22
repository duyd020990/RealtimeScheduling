#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* SWITCH */
#define TBS      1
#define TBS_VRA  1
#define TBS_EVRA 1
#define ATBS     1
#define ATBS_VRA 1
#define ATBS_EVRA 1
#define ITBS     1
#define CBS      1

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

/* General Definitions */
#define MAX_TASKS        100
#define MAX_INSTANCES  50000
#define TICKS         100000

#define  C0_cbs_Q_DIV  1

/* Definitions for Adaptive TBS */
unsigned int  C0;          /* 1st PET */
#define       Ci   1       /* Following PET */
#define pre_alpha	0.5	// prediction coefficient


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
#define min(a,b) ((a) < (b) ? (a) : (b))


/***********************************************************************************************************/
/* TCB                                                                                                     */
/***********************************************************************************************************/
typedef struct tcb {
  int                tid;
  int                inst_no;
  unsigned long long req_tim;
  unsigned long long req_tim_advance;
  unsigned long long a_dl;
  unsigned long long a_dl_TBS;
  unsigned int       et;
  unsigned int       initial_et;
  unsigned int       pet;
  struct tcb         *next;
  struct tcb         *prev;
} TCB;


/***********************************************************************************************************/
/* Variables                                                                                               */
/***********************************************************************************************************/

unsigned long long  tick;
unsigned int        et[MAX_TASKS][MAX_INSTANCES];       /* $B3F%?%9%/$N%$%s%9%?%s%9Kh$N<B9T;~4V(B             */
unsigned int        wcet[MAX_TASKS];                    /* $B3F%?%9%/$N%$%s%9%?%s%9Kh$N:G0-<B9T;~4V(B         */
unsigned int        inst_no[MAX_TASKS];                 /* $B3F%?%9%/$N8=:_$N%$%s%9%?%s%9HV9f(B               */
unsigned int        periodic[MAX_TASKS];                /* $B3F%?%9%/$,<~4|%?%9%/$+Hs<~4|%?%9%/$+(B           */
unsigned long long  period[MAX_TASKS];                  /* $B<~4|%?%9%/$N<~4|!%Hs<~4|%?%9%/$O%<%m(B           */
unsigned long long  phase[MAX_TASKS];                   /* Phase of periodic tasks                        */
unsigned long long  req_tim[MAX_TASKS][MAX_INSTANCES];  /* $B3F%?%9%/$N3+;O;~9o(B                             */
unsigned int        exec_times[MAX_TASKS];              /* $B3F%?%9%/$N<B9T2s?t(B                             */
unsigned int        aperiodic_exec_times;               /* $BHs<~4|%?%9%/A4BN$N<B9T2s?t(B                     */
unsigned int        aperiodic_total_et;                 /* $BHs<~4|%?%9%/A4BN$N%H!<%?%k<B9T;~4V(B             */
double              aperiodic_response_time;            /* $BHs<~4|%?%9%/$NJ?6Q1~Ez;~4V(B                     */
double              p_util=0.0;                         /* $B<~4|%?%9%/72$N%H!<%?%k;HMQN((B                   */
unsigned long long  pre_et[MAX_TASKS][MAX_INSTANCES];	/* Predicted execution times								*/
unsigned long long  prev_sum_et[MAX_TASKS][MAX_INSTANCES];	/* sum of previous execution times			                    */


TCB                *p_ready_queue, *ap_ready_queue;
TCB                *fifo_ready_queue;
unsigned int        p_tasks, ap_tasks;
TCB                *_kernel_runtsk = NULL, *_kernel_runtsk_pre = NULL;

/* Variables for TBS95 */
unsigned long long  di_1, fi_1;

/* Variables for VRA */
unsigned long long  max_used_dl;
unsigned long long  used_dl[TICKS];
unsigned int        last_empty;                         /* $B:G8e$N6u%9%m%C%HHV9f(B                           */

/*Vairables for SVRA*/
unsigned long long  max_period;							/* the maximum period of tasks*/
unsigned int		Tmax_last_used;						/* the time when the last instance of the task having maximum period is stoped. */
unsigned int		ins_start[MAX_TASKS*MAX_INSTANCES];	/* Starting time of all of the instance executed */
unsigned int		ins_start_dl[MAX_TASKS*MAX_INSTANCES];	/* Starting time of all of the instance executed */
unsigned int		ins_num;							/* the number of instances */
unsigned long long  a_dl_bound;
int 				pre_ins_id;
/*unsigned long long  used_dl[TICKS];*/					/* Using the same with VRA*/
/*unsigned int        last_empty;*/						/* Using the same with VRA*/

/*Vairables for Adaptive TBS*/


/* Variables for TB* / TB(n) */

/* Variables for CBS */
unsigned long long  cbs_d;
unsigned int        cbs_c;


/* Variables for overheads estimation */
unsigned long long overhead_dl, overhead_dl_max, overhead_dl_total;
unsigned long long overhead_alpha, overhead_alpha_max, overhead_alpha_total;
unsigned long long overhead_total, overhead_total_max, overhead_total_full;


/* Prototype */
void   insert_queue ( TCB **rq, TCB *entry );
void   insert_queue_fifo ( TCB **rq, TCB *entry );
void   delete_queue ( TCB **rq );
void   from_fifo_to_ap ( void );
void   Scheduling ( void );
void   Initialize ( void );
void   Tick_inc ( void );
TCB   *entry_set ( int );
void   Job_exit ( const char *, int );
void   Overhead_Record ( void );
void   DL_set_TBS ( TCB *, unsigned int );
void   DL_set_TBS_vra0 ( TCB *, unsigned int, unsigned long long, unsigned long long );
void   DL_set_TBS_vra ( TCB *, unsigned int );
void   DL_set_CBS ( TCB * );
void   DL_update_ATBS ( TCB *, unsigned int, unsigned int);
void   DL_update_CBS ( TCB * );
void   DL_set_ITBS ( TCB * );
void   DL_set_TBS_evra (TCB *, unsigned int);