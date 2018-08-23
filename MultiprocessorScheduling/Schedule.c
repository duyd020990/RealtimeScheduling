#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "Schedule.h"

#include "RM.h"
#include "LSF.h"
#include "EDF.h"
#include "LLREF.h"

//#define AP_ALSO

#if LSF == 1
unsigned long long migration;
extern SCHEDULING_ALGORITHM LSF_sa;
#endif

#if RM == 1
extern SCHEDULING_ALGORITHM RM_sa;
#endif

#if EDF == 1
extern SCHEDULING_ALGORITHM EDF_sa;
#endif

#if LLREF == 1
extern int release_time[TICKS];
extern int assign_history[MAX_TASKS];
extern SCHEDULING_ALGORITHM LLREF_sa;
extern LLREF_LRECT* lrect_queue;
#endif

/***********************************************************************************************************/
/* Variables                                                                                               */
/***********************************************************************************************************/

unsigned long long  tick;
unsigned            et[MAX_TASKS][MAX_INSTANCES];       /* 各タスクのインスタンス毎の実行時間             */
unsigned            wcet[MAX_TASKS];                    /* 各タスクのインスタンス毎の最悪実行時間         */
unsigned            inst_no[MAX_TASKS];                 /* 各タスクの現在のインスタンス番号               */
unsigned            periodic[MAX_TASKS];                /* 各タスクが周期タスクか非周期タスクか           */
unsigned long long  period[MAX_TASKS];                  /* 周期タスクの周期．非周期タスクはゼロ           */
unsigned long long  phase[MAX_TASKS];                   /* Phase of periodic tasks                        */
unsigned long long  req_tim[MAX_TASKS][MAX_INSTANCES];  /* 各タスクの開始時刻                             */
unsigned            exec_times[MAX_TASKS];              /* 各タスクの実行回数                             */
unsigned            aperiodic_exec_times;               /* 非周期タスク全体の実行回数                     */
unsigned            aperiodic_total_et;                 /* 非周期タスク全体のトータル実行時間             */
double              aperiodic_response_time;            /* 非周期タスクの平均応答時間                     */
double              p_util=0.0;                         /* 周期タスク群のトータル使用率                   */
double              job_util[MAX_TASKS];

TCB                *p_ready_queue, *ap_ready_queue;
TCB                *fifo_ready_queue;
unsigned            p_tasks, ap_tasks;
TCB                *_kernel_runtsk[PROCESSOR_NUM] = {NULL}, *_kernel_runtsk_pre[PROCESSOR_NUM] = {NULL};

/* Variables for TBS95 */
unsigned long long  di_1, fi_1;

/* Variables for VRA */
unsigned long long  max_used_dl;
unsigned long long  used_dl[PROCESSOR_NUM][TICKS];
unsigned            last_empty[PROCESSOR_NUM];                         /* 最後の空スロット番号                           */

/* Variables for TB* / TB(n) */

/* Variables for CBS */
unsigned long long  cbs_d;
unsigned            cbs_c;


/* Variables for overheads estimation */
unsigned long long overhead_dl, overhead_dl_max, overhead_dl_total;
unsigned long long overhead_alpha, overhead_alpha_max, overhead_alpha_total;

int has_new_task;
int has_new_instance;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main ( int argc, char *argv[] )
{

    FILE              *p_cfp;
#ifdef AP_ALSO
    FILE              *ap_cfp;
#endif
    FILE              *ovhd_dl_max_fp, *ovhd_al_max_fp, *ovhd_dl_total_fp, *ovhd_al_total_fp;
    int                i, j;
    char               buf[BUFSIZ];
    unsigned           task_id, task_wcet, task_et, prev_task_id=MAX_TASKS;
    unsigned long long task_period, task_req_tim;

#if LLREF == 1
    memset(release_time,0,sizeof(int)*TICKS);
#endif

    if ( (p_cfp = fopen ( "periodic.cfg", "r") ) == NULL ) {
        printf ( "Cannot open \"periodic.cfg\"\n" );
        return 1;
    }

#ifdef AP_ALSO
    if ( (ap_cfp = fopen ( "aperiodic.cfg", "r") ) == NULL ) {
        printf ( "Cannot open \"aperiodic.cfg\"\n" );
        return 1;
    }
#endif

    if ( ( ovhd_dl_max_fp = fopen ( "ovhd_dl_max.csv", "a" ) ) == NULL ) {
        printf ( "Cannot open \"ovhd_dl_max.csv\"\n" );
        return 1;
    }
    fseek ( ovhd_dl_max_fp, 0, SEEK_END );

    if ( ( ovhd_dl_total_fp = fopen ( "ovhd_dl_total.csv", "a" ) ) == NULL ) {
        printf ( "Cannot open \"ovhd_dl_total.csv\"\n" );
        return 1;
    }
    fseek ( ovhd_dl_total_fp, 0, SEEK_END );

    if ( ( ovhd_al_max_fp = fopen ( "ovhd_al_max.csv", "a" ) ) == NULL ) {
      printf ( "Cannot open \"ovhd_al_max.csv\"\n" );
      return 1;
    }
    fseek ( ovhd_al_max_fp, 0, SEEK_END );

    if ( ( ovhd_al_total_fp = fopen ( "ovhd_al_total.csv", "a" ) ) == NULL ) {
      printf ( "Cannot open \"ovhd_al_total.csv\"\n" );
      return 1;
    }
    fseek ( ovhd_al_total_fp, 0, SEEK_END );
  

    for ( i = 0; i < MAX_TASKS; i++ ) {
        for ( j = 0; j < MAX_INSTANCES; j++ ) {
            req_tim[i][j] = 0xFFFFFFFFFFFFFFFF;
        }
    }

  /* Inputting periodic tasks' information */
    while ( fgets ( buf, BUFSIZ, p_cfp ) != NULL ) 
    {
        if ( buf[0] == '#' )
        {
            continue;
        }
        sscanf ( buf, "%d\t%d\t%d\t%llu\t%llu\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

        wcet[task_id]                        = task_wcet;
        et[task_id][inst_no[task_id]]        = task_wcet;    /* At the moment, WCET is set for periodic tasks */
        req_tim[task_id][inst_no[task_id]++] = task_req_tim;
#if LLREF == 1
        release_time[task_req_tim]=1;
#endif
        if ( task_id != prev_task_id ) 
        {
            phase[task_id]    = task_req_tim;
            prev_task_id      = task_id;
            periodic[task_id] = 1;
            period[task_id]   = task_period;
            job_util[task_id] = (double)task_wcet / (double)task_period;
            p_util            += job_util[task_id];
            p_tasks++;
        }
    }

    for ( i = 0; i < MAX_TASKS; i++ )
        inst_no[i] = 0;

#ifdef AP_ALSO
  /* Inputting aperiodic tasks' information */
    while ( fgets ( buf, BUFSIZ, ap_cfp ) != NULL ) 
    {
        double dummy1, budget;
        int    dummy2;

        if ( strncmp ( buf, "#average", 8 ) == 0 ) 
        {
            sscanf ( buf, "#average_interval=%lf, average_wcet=%d, average_et=%lf", &dummy1, &dummy2, &budget );
            C0 = cbs_Q = (unsigned)ceil (budget/C0_cbs_Q_DIV);
        }
        if ( buf[0] == '#' )
        {
            continue;
        }
        sscanf ( buf, "%d\t%d\t%d\t%llu\t%llu\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

        wcet[task_id]                        = task_wcet;
        et[task_id][inst_no[task_id]]        = task_et;
        req_tim[task_id][inst_no[task_id]++] = task_req_tim;
        ap_tasks++;
    }
    fclose ( ap_cfp );
#endif

    fclose ( p_cfp );

    if ( p_util > UTIL_UPPERBOUND ) 
    {
        printf ( "Cannot execute tasks over 100%% utilization\n" );
        return 0;
    }

/***************************************************************/
// All periodic tasks ordered by deadline, this means it use EDF algorithm,
// All aperiodic tasks oredered by when it comes.
/***************************************************************/

/****************************************************************************************************************/
/* RM                                                                                                          */
/****************************************************************************************************************/
#if RM == 1
    run_simulation("RM",RM_sa);
  
    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    printf ( ", ", aperiodic_response_time );

    fprintf ( ovhd_dl_max_fp, "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp, "," );
    fprintf ( ovhd_al_total_fp, "," );

#endif

#if LSF == 1
    
    run_simulation("LSF",LSF_sa);
    
    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp  , "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp  , "," );
    fprintf ( ovhd_al_total_fp, "," );

#endif

#if EDF == 1
    
    run_simulation("EDF",EDF_sa);

    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp  , "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp  , "," );
    fprintf ( ovhd_al_total_fp, "," );
#endif



    fclose ( ovhd_dl_max_fp );
    fclose ( ovhd_dl_total_fp );
    fclose ( ovhd_al_max_fp );
    fclose ( ovhd_al_total_fp );

    return 0;
}


void run_simulation(char* s,SCHEDULING_ALGORITHM sa)
{
    int i,processor_id;
    void (*scheduling)() = NULL;
    void (*reorganize_function)(TCB**) = NULL;
    TCB *entry;

    if(sa.scheduling==NULL || sa.insert_OK==NULL || sa.reorganize_function==NULL)
    {
        return;
    }
    scheduling = (void (*)())sa.scheduling;
    reorganize_function = (void (*)(TCB**))sa.reorganize_function;

    Initialize ();

    for ( tick = 0; tick < TICKS; ) 
    {
        for ( i = 0; i < MAX_TASKS; i++ ) 
        {

            if ( req_tim[i][inst_no[i]] == tick ) 
            {
                entry = entry_set ( i );
                if ( periodic[i] == 1 ) 
                {
                    entry -> a_dl = tick + period[i];
                    insert_queue( &p_ready_queue, entry ,sa.insert_OK);
                    if(phase[i] == tick)
                    {
                        has_new_task = 1;
                    }
                }
                else 
                {
                    insert_queue_fifo ( &fifo_ready_queue, entry );
                }
                inst_no[i]++;
                has_new_instance = 1;
            }
        }
        reorganize_function(&p_ready_queue);

        if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) 
        {
            from_fifo_to_ap ();
        }
                
        /* Scheduling */
        scheduling();

        /* Tick increment/et decrement and Setting last_empty & used_dl*/
        Tick_inc ();
        for(processor_id=0;processor_id<PROCESSOR_NUM;processor_id++)
        {
            if ( _kernel_runtsk[processor_id] != NULL ) 
            {
                if ( _kernel_runtsk[processor_id] -> et == 0 ) 
                {
                    Job_exit ( s, 1 ,processor_id);
                }
            }
        }
    }
}

void insert_queue( TCB **rq,TCB *entry,void* function)
{
    TCB  *p;
    int (*insert_OK)(TCB* t1,TCB* t2) = NULL;

    if(function==NULL)
    {
        return;
    }
    insert_OK = (int (*)(TCB*,TCB*)) function;

    for ( p = *rq; p != NULL; p = p -> next )
    {
        if (insert_OK(entry,p)) 
        {
            if ( p == *rq ) 
            {
                entry -> next = p;
                p -> prev = entry;
                *rq = entry;
            }
            else 
            {
                p -> prev -> next = entry;
                entry -> next = p;
                entry -> prev = p -> prev;
                p -> prev     = entry;
            }
            break;
        }
        if ( p -> next == NULL ) 
        {
            p -> next     = entry;
            entry -> prev = p;
            break;
        }
    }
    if ( *rq == NULL ) 
    {
        *rq = entry;
    }

    return;
}


void insert_queue_fifo ( TCB **rq, TCB *entry )
{
    TCB  *p;

    if ( *rq == NULL ) 
    {
        *rq = entry;
        entry -> next = NULL;
        entry -> prev = NULL;

        return;
    }

    for ( p = *rq; p -> next != NULL; p = p -> next ); /* loop until tail */

    p -> next = entry;
    entry -> next = NULL;
    entry -> prev = p;

    return;
}

void from_fifo_to_ap ( void )
{
    ap_ready_queue   = fifo_ready_queue;
    fifo_ready_queue = fifo_ready_queue -> next;
    ap_ready_queue -> next = ap_ready_queue -> prev = NULL;
    if ( fifo_ready_queue != NULL )
    {
        fifo_ready_queue -> prev = NULL;
    }

    return;
}

void delete_queue ( TCB **rq,int tid )
{
    TCB *p;
    TCB* h_p;
    p = *rq;
    for(;p!=NULL;p=p->next)
    {
        if(p->tid == tid)
        {
            h_p = p->prev;
            if(h_p == NULL)
            {
                *rq = (*rq) -> next; 
                if(*rq!=NULL)
                {
                    (*rq)->prev = NULL;
                }
            }
            else if(p->next == NULL)
            {
                p->prev->next = NULL;
            }
            else
            {
                h_p->next = p->next;
                p->next->prev = h_p;
            }
            break;
        }
    }

    free ( p );

    return;
}


void Initialize ( void )
{
    int i,j;

    aperiodic_exec_times    = 0;
    aperiodic_total_et      = 0;
    aperiodic_response_time = 0;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        _kernel_runtsk[i] = _kernel_runtsk_pre[i] = NULL;
    }

    for ( i = 0; i < MAX_TASKS; i++ ) 
    {
        exec_times[i] = 0;
        inst_no[i] = 0;
        job_util[i] = 0.0;
    }

    p_ready_queue    = NULL;
    ap_ready_queue   = NULL;
    fifo_ready_queue = NULL;

#if LLREF == 1
    lrect_queue      = NULL;
    for(i=0;i<MAX_TASKS;i++)
    {
        assign_history[i] = -1;
    }
#endif

    di_1 = 0;
    fi_1 = 0;
    for(j=0;j<PROCESSOR_NUM;j++)
    {
        for ( i = 0; i < TICKS; i++ ) 
        {
            used_dl[j][i] = 0;
        }   
    }
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        last_empty[i]  = 0;
    }
    max_used_dl = 0;

    cbs_d = 0;
    cbs_c = cbs_Q;

    overhead_dl = overhead_alpha = overhead_dl_max = overhead_alpha_max = overhead_dl_total = overhead_alpha_total = 0;

    return;
}

TCB *entry_set ( int i)
{
    TCB *entry;

    entry =  malloc ( sizeof ( TCB ) );
    entry -> tid             = i;
    entry -> inst_no         = inst_no[i];
    entry -> req_tim         = tick;
    entry -> req_tim_advance = tick;
    entry -> et              = et[i][inst_no[i]];
    entry -> initial_et      = et[i][inst_no[i]];
    entry -> next = entry -> prev = NULL;
    entry -> pet = C0;   // may be reset in the appropriate function
  
    entry -> a_dl = (unsigned long long)0xFFFFFFFFFFFFFFFF;  // dummy necessary
  
    return entry;
}



void Tick_inc ( )
{
    int i,active=0;
    
    for(i=0;i<PROCESSOR_NUM;i++)
    {
     
        if ( _kernel_runtsk[i] == NULL )
        {
            last_empty[i] = tick;
        }
        else
        {
            used_dl[i][tick] = _kernel_runtsk[i] -> a_dl;
        }
      
        Overhead_Record ( );
    }

    tick++;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if ( _kernel_runtsk[i] != NULL ) 
        {
            active++;
            --(_kernel_runtsk[i] -> et);
            --(_kernel_runtsk[i] -> pet);
        }   
    }
    return;
}


void Overhead_Record ( void )
{

    if ( overhead_dl_max < overhead_dl )
    {
      overhead_dl_max = overhead_dl;
    }
  
    if ( overhead_alpha_max < overhead_alpha )
    {
      overhead_alpha_max = overhead_alpha;
    }

    overhead_dl_total    += overhead_dl;
    overhead_alpha_total += overhead_alpha;

    overhead_dl = overhead_alpha = 0;
  
    return;
}


void Job_exit ( const char *s, int rr, int processor_id) /* rr: resource reclaiming */
{
    if(_kernel_runtsk[processor_id] == NULL)
    {
        return;
    }

    if ( _kernel_runtsk[processor_id] -> a_dl < tick ) 
    {
        fprintf( stderr, "\n# MISS: %s: tick=%lld: DL=%lld: DL_ORIG=%lld: TID=%d: INST_NO=%d, WCET=%d, ET=%d, req_tim=%lld, req_tim_advance=%lld\n",
           s, tick, _kernel_runtsk[processor_id]->a_dl, _kernel_runtsk[processor_id]->a_dl_TBS, _kernel_runtsk[processor_id]->tid, _kernel_runtsk[processor_id]->inst_no,
           wcet[_kernel_runtsk[processor_id]->tid], _kernel_runtsk[processor_id]->initial_et, _kernel_runtsk[processor_id]->req_tim, _kernel_runtsk[processor_id]->req_tim_advance);
        printf( "\n# MISS: %s: tick=%lld: DL=%lld: DL_ORIG=%lld: TID=%d: INST_NO=%d\n",
           s, tick, _kernel_runtsk[processor_id]->a_dl, _kernel_runtsk[processor_id]->a_dl_TBS, _kernel_runtsk[processor_id]->tid, _kernel_runtsk[processor_id]->inst_no );
    }

    exec_times[ _kernel_runtsk[processor_id] -> tid]++;

    if ( periodic[_kernel_runtsk[processor_id]->tid] == 0 ) 
    {
        aperiodic_exec_times++;
        aperiodic_total_et += tick - _kernel_runtsk[processor_id] -> req_tim;
        aperiodic_response_time = (double)aperiodic_total_et / (double)aperiodic_exec_times;

        if ( rr == 1 ) 
        {/* Trying resource reclaiming */
            overhead_dl += COMP + COMP + FDIV + CEIL + IADD; /* Of course, (1.0 - p_util) is statically solved. */
            di_1 = (double)max3(_kernel_runtsk[processor_id]->req_tim_advance, di_1, fi_1) + ceil ( (double)_kernel_runtsk[processor_id]->initial_et/ (UTIL_UPPERBOUND - p_util) );
        }
        else 
        { /* Not concerned with CBS */
            overhead_dl += ASSIGN;
            di_1 = _kernel_runtsk[processor_id] -> a_dl_TBS;
        }
        overhead_dl += ASSIGN;
        fi_1 = tick;
    }

    if ( periodic[_kernel_runtsk[processor_id]->tid] == 1 )
    {
        delete_queue ( &p_ready_queue,_kernel_runtsk[processor_id]->tid );
    }
    else
    {
        delete_queue ( &ap_ready_queue,_kernel_runtsk[processor_id]->tid );
    }
    _kernel_runtsk[processor_id] = NULL;
    return;
}

