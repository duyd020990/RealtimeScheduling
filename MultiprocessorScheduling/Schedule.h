#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* Debug Configuration */
#define debug	0
#define db_MEM	0

/* Scheduler Switch */
#define LAA      0
#define PFAIR	 0
#define RUN      1

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
#define MAX2     2
#define MIN2     2
#define MAX3     3
#define MIN3     3
#define TCBSET   8

/* General Definitions */
#define MAX_PRO			  4
#define MAX_TASKS        110
#define MAX_INSTANCES  100000
#define TICKS          100000

/* Macros */
#define max(a,b) ((a) > (b) ? (a) : (b))
#define max3(a,b,c) ((a) > (b) ? ((a) > (c) ? (a) : (c)) : ((b) > (c) ? (b) : (c)))
#define min3(a,b,c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
#define min(a,b) ((a) < (b) ? (a) : (b))


/***********************************************************************************************************/
/* TCB                                                                                                     */
/***********************************************************************************************************/
typedef struct tcb {
  int                iTaskID;
  int                iInstanceNo;
  unsigned long long ullRequestTime;
  unsigned long long ullAbsoluteDealine;
  unsigned int       uiET;
  //unsigned int       pet;
  struct tcb         *pNext;
  struct tcb         *pPrev;
} TCB;

/****************************************************************************************************************/
/* ADL: Absolute Deadline Cell                                                                             		*/
/****************************************************************************************************************/
typedef struct adl {
  int                val;
  struct adl         *pNext;
  struct adl         *pPrev;
} ADL;

/****************************************************************************************************************/
/* TID: Task ID                                                                                 				*/
/****************************************************************************************************************/
typedef struct tid {
  int                iTaskID;
  struct tid         *pNext;
  struct tid         *pPrev;
} TID;

/****************************************************************************************************************/
/* MAP_CELL: mapping cell                                                                                 		*/
/****************************************************************************************************************/
typedef struct map_cell {
  int                iTaskID;
  double			 dUtilization;
  unsigned int		 uiET;
  struct map_cell    *pNext;
  struct map_cell    *pPrev;
} MAP_CELL;

/****************************************************************************************************************/
/* SCB: Server Control Block                                                                                 	*/
/****************************************************************************************************************/
typedef struct scb {
  unsigned int       uiLeafNo;
  unsigned int		 uiIsPacked;		// 0: by Dual; 1: by Packing
  unsigned int		 uiET;
  double			 dUtilization;
  unsigned long long ullADL;
  TID				 *pTID;
  struct scb         *pLeaf;
  struct scb         *pRoot;
  struct scb         *pNext;
  struct scb         *pPrev;
} SCB;

/****************************************************************************************************************/
/* SERLIST: Server list                                                                                 		*/
/****************************************************************************************************************/
typedef struct serlist {
  SCB                	*scb;
  struct serlist         *pNext;
  struct serlist         *pPrev;
} SERLIST;

/****************************************************************************************************************/
/* Common variables                                                                                        		*/
/****************************************************************************************************************/

unsigned long long  ullTick;									/* Time tick									*/
unsigned int        uiExeTime[MAX_TASKS][MAX_INSTANCES];		/* Execution time of instances of tasks			*/
unsigned int        uiWCET[MAX_TASKS];                    		/* Tasks' worst-case execution time				*/
unsigned int        uiInstanceNo[MAX_TASKS];                 	/* Number of instances							*/
int		        	iIsPeriodic[MAX_TASKS];                		/* Is periodic task?							*/
unsigned long long  ullPeriod[MAX_TASKS];                  		/* Periodic of tasks							*/
unsigned long long  ullPhase[MAX_TASKS];                   		/* Phase of periodic tasks						*/
unsigned long long  ullRequestTime[MAX_TASKS][MAX_INSTANCES];	/* Request time of instances of tasks			*/
unsigned int        uiInstantExeTimes[MAX_TASKS];				/* Execution time of each instance of tasks		*/
double              dPeriodicUtil[MAX_TASKS];					/* Processor utilization of periodic tasks		*/
double				dPeriodicUtilSum 	= 0;					/* Total utilization of periodic tasks			*/
int 				iPrevScheduled[MAX_TASKS];					/* The last processor on which task is executed	*/

TCB					*pTCBReadyQueue[MAX_PRO];					/* The ready queue of processors				*/
TCB					*pTCBWaitingQueue;							/* The global waiting queue						*/
TCB					*pTCBKernelRuntsk[MAX_PRO];					/* The running task on processors				*/
TCB					*pTCBKernelRuntskPre[MAX_PRO];				/* The previous task running on processors		*/
TCB					*pAWQ;										/* The FIFS queue of aperiodic  waiting jobs	*/
TCB					*pARQ;										/* The FIFS queue of aperiodic ready jobs		*/

unsigned int 		uiPTaskNo;									/* The number of periodic task					*/
unsigned int 		uiATaskNo;									/* The number of aperiodic task					*/
unsigned int 		uiAInstanceNo;								/* The number of aperiodic instance				*/
unsigned int 		uiTotalInsNo;								/* The total number of instances released		*/

unsigned int 		uiPreAssign[MAX_TASKS]; 					/* The last assigned processor of tasks			*/

unsigned int 		uiSchedulerCout;							/* Count of scheduler invocations				*/
unsigned int 		uiPreemptionCout;							/* Count of preemption occurrences				*/
unsigned int 		uiMigrationCout;							/* Count of migrations							*/
unsigned int 		uiOverhead;									/* The accumulative overhead 					*/
unsigned int 		uiOverheadMax;								/* The maximum runtime overhead per tick		*/
unsigned int 		uiAResponseSum;								/* Total response time of aperiodic jobs		*/
double				dAverageResponse;							/* Average response time of aperiodic jobs		*/

unsigned int 		apFinishedNo;								/* The number of aperiodic instance finished	*/

/****************************************************************************************************************/
/* RUN variables                                                                                           		*/
/****************************************************************************************************************/
unsigned int uiSelectedNo;										/* The number of server selected for execution	*/
unsigned int uiLastAssign[MAX_TASKS];							/* The last PID on which the TID was assigned	*/
unsigned int uiAssigned[MAX_PRO]; 								/* Whether PID is assigned a task (1) or not (0)*/
int iHasServerFinished;											/* Is there a server just finished. Active by 1 */
int iHasJobFinished;											/* Is there a job just finished. Active by 1 	*/
SCB	*pTopSer;													/* The list of top (root) servers				*/
SCB *pSCBReadyQueue[MAX_PRO];									/* The list of servers ready for execution		*/
SERLIST *pSelectedServer;										/* The list of server selected for execution	*/


/****************************************************************************************************************/
/* Prototypes                                                                                             		*/
/****************************************************************************************************************/
// Common functions
void	Initialize(void);											/* Initializing the system								*/
void	fnRunScheduling(void);										/* RUN: Scheduling										*/
void	fnRunTickInc(void);											/* RUN: Tick increment									*/
void	fnRunJobComplete(const char *s, TCB *tcb);					/* RUN: Job complete event_callback						*/ 
void	fnRunJobRemove(TCB *tcb, int periodic);						/* RUN: Remove the finished job from the waiting queue	*/
TCB		*entry_set(int);											/* RUN: Create a new TCB entry							*/
TID		*tid_set( int tid);											/* RUN: Create a new TID entry							*/
TCB		*fnSeekTCB(TCB **wq, unsigned int uiTaskID);				/* RUN: Seeking for a task in the waiting queue			*/

double	round(double num);											/* RUN: Rounding numbers to decimal ones with the 1st place	*/
void	fnEvaluation();												/* RUN: Calculating evaluation criteria required		*/
void	Record(void);												/* RUN: Recording (saving) evaluation criteria to files	*/				
void	fnFprintServer(SCB *server, const char *s);					/* RUN: Print a server to the screen					*/
void	fnFprintTCBReady(const char *s);							/* RUN: Print the task list to the screen				*/
void	fnFprintServerList(SCB *serverList, const char *s);			/* RUN: Print a server list to the screen				*/

// Functions of RUN
void	fnRunInsertWaitingQueue(TCB **wq, TCB *pTCBEntry);			/* RUN: Insert a periodic task into the waiting queue	*/
void	fnRunInsertAperiodic(TCB *tcb);								/* RUN: Insert an aperiodic task into the aperiodic queue*/
int		fnRunJobToRQ(TCB **aq, TCB *tcb);							/* RUN: Move a job from the waiting queue to the ready one*/
void	fnRunJobToWQ(TCB **aq, TCB *tcb);							/* RUN: Move a job to the waiting queue					*/
void	fnBuildReductionTree(SCB **topRoot, TCB **wq);				/* RUN: Build the reduction tree						*/
SCB		*fnBuildServers(TCB **wq);									/* RUN: Create servers from existing tasks				*/
SCB		*fnSCBByTCB(TCB *entry);									/* RUN: Create a server by a TCB task					*/
SCB		*fnSCBDummy(double ultil);									/* RUN: Create a server by a dummy task					*/
void	fnSCB2List(SCB **scbq, SCB *pSCBEntry);						/* RUN: Add a server to a list							*/
SCB		*fnGetSCB(SCB **scbq);										/* RUN: Get the first server of a list					*/
void	fnSCB2Pack(SCB **root, SCB *leaf);							/* RUN: Add a server to a Packing						*/
void	fnAddTID(TID **tidq, TID *pTIDEntry);						/* RUN: Add a TID to a list								*/
int 	get_tid_from_server(SCB *scb);								/* RUN: Get TID of a server								*/

SCB		*fnPack(SCB **SCBList);										/* RUN: Packing operation								*/
SCB		*fnDual(SCB **serverList);									/* RUN: Dual operation									*/
SCB		*fnDualSet(SCB *Entry);										/* RUN: Create a dual server							*/
void	fnRemoveProperSubsystem(SCB **topRoot, SCB **packedList);	/* RUN: Obtain a proper subsystem						*/
void	fnSlitSCB(SCB **scbq, SCB *slitted);						/* RUN: Slit a server from a list						*/
void	fnFreeServerList(SERLIST **list);							/* RUN: Deallocate a server list						*/

void	fnSearchRUN (SCB **root);									/* RUN: Search on the reduction tree for task selection	*/
void	fnSearchNode(SCB *searchNode, int iSelected);				/* RUN: Search on a node for task selection				*/
void	fnAdd2SelectedServers(SCB *selected);						/* RUN: Add a server as selected ones					*/
void 	fnAdd2ReadyServers(SCB *selected);							/* RUN: Move selected servers to ready queue			*/

void	fnRUNAssign();												/* RUN: Assign selected servers to processors			*/
void	fnUpdateReductionTree(SCB **root);							/* RUN: Update the reduction tree after execution		*/
void	fnUpdateNode(SCB *Node);									/* RUN: Update a node after execution					*/
void	fnRunTickUpdateSelectedServers(SERLIST *selected);			/* RUN: Update the list of selected servers				*/

