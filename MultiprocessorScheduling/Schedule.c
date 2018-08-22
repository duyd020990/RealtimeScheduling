#include "Schedule.h"
#include <stdio.h>

int main ( int argc, char *argv[] )
{
	//printf ( "Starting running\n" );
	FILE              *pCFP, *apCFP;
	FILE              *migration_fp, *preemption_fp, *overhead_fp, *response_fp;
	int                i, j, ch;
	char               cBUF[BUFSIZ];
	unsigned int       uiTaskID, uiTaskWCET, uiTaskET, uiPrevTaskID=MAX_TASKS;
	unsigned long long ullTaskPeriod, ullTaskRequestTime;
	TCB               *pTCBEntry;

	if ( (pCFP = fopen ( "periodic.cfg", "r") ) == NULL ) 
	{
		printf ( "Cannot open \"periodic.cfg\"\n" );
		return 1;
	}
  
	if ( (apCFP = fopen ( "aperiodic.cfg", "r") ) == NULL ) 
	{
		printf ( "Cannot open \"aperiodic.cfg\"\n" );
		return 1;
	}
  
	if ( ( migration_fp = fopen ( "migration.csv", "a" ) ) == NULL ) 
	{
		printf ( "Cannot open \"migration.csv\"\n" );
		return 1;
	}
	fseek ( migration_fp, 0, SEEK_END );
  
	if ( ( preemption_fp = fopen ( "preemption.csv", "a" ) ) == NULL ) 
	{
		printf ( "Cannot open \"preemption.csv\"\n" );
		return 1;
	}
	fseek ( preemption_fp, 0, SEEK_END );
  
	if ( ( overhead_fp = fopen ( "overhead.csv", "a" ) ) == NULL ) 
	{
		printf ( "Cannot open \"overhead.csv\"\n" );
		return 1;
	}
	fseek ( overhead_fp, 0, SEEK_END );
  
	if ( ( response_fp = fopen ( "response.csv", "a" ) ) == NULL ) 
	{
		printf ( "Cannot open \"response.csv\"\n" );
		return 1;
	}
	fseek ( response_fp, 0, SEEK_END );
  
	for ( i = 0; i < MAX_TASKS; i++ ) 
	{
		for ( j = 0; j < MAX_INSTANCES; j++ ) 
		{
			ullRequestTime[i][j] = 0xFFFFFFFFFFFFFFFF;
		}
	}

	/* Inputting periodic tasks' information */
	while ( fgets ( cBUF, BUFSIZ, pCFP ) != NULL ) 
	{
		if ( cBUF[0] == '#' )
			continue;
		sscanf ( cBUF, "%d\t%d\t%d\t%ld\t%ld\n", &uiTaskID, &uiTaskWCET, &uiTaskET, &ullTaskPeriod, &ullTaskRequestTime );
		
		uiWCET[uiTaskID]                        			= uiTaskWCET;
		uiExeTime[uiTaskID][uiInstanceNo[uiTaskID]]        	= uiTaskET;
		ullRequestTime[uiTaskID][uiInstanceNo[uiTaskID]++] 	= ullTaskRequestTime;

		if ( uiTaskID != uiPrevTaskID ) 
		{
			ullPhase[uiTaskID]    	= ullTaskRequestTime;
			uiPrevTaskID      		= uiTaskID;
			iIsPeriodic[uiTaskID] 	= 1;
			ullPeriod[uiTaskID]   	= ullTaskPeriod;
			dPeriodicUtil[uiTaskID]	= round((double)uiTaskWCET / (double)ullTaskPeriod);
			dPeriodicUtilSum		+= dPeriodicUtil[uiTaskID];
			uiPTaskNo++;
		}
	}
  
	/* Inputting aperiodic tasks' information */
	while ( fgets ( cBUF, BUFSIZ, apCFP ) != NULL ) 
	{
		if ( cBUF[0] == '#' )
			continue;
		sscanf ( cBUF, "%d\t%d\t%d\t%ld\t%ld\n", &uiTaskID, &uiTaskWCET, &uiTaskET, &ullTaskPeriod, &ullTaskRequestTime );

		uiWCET[uiTaskID]                        			= uiTaskWCET;
		uiExeTime[uiTaskID][uiInstanceNo[uiTaskID]]        	= uiTaskET;
		ullRequestTime[uiTaskID][uiInstanceNo[uiTaskID]++] 	= ullTaskRequestTime;
		if(uiTaskID != uiPrevTaskID)
		{
			ullPhase[uiTaskID]    	= ullTaskRequestTime;
			uiPrevTaskID      		= uiTaskID;
			iIsPeriodic[uiTaskID] 	= 0;
			uiATaskNo++;
		}
		
	}

	for ( i = 0; i < MAX_TASKS; i++ )
		uiInstanceNo[i] = 0;

	fclose ( pCFP );
	fclose ( apCFP );
  
	if ( round(dPeriodicUtilSum) > MAX_PRO ) 
	{
		printf ( "Cannot execute tasks over 100%% utilization. Total utilization: %f \n", dPeriodicUtilSum);
		fprintf (stderr, "System overload u= %0.1lf\n",dPeriodicUtilSum);
		for (i=0; i<uiPTaskNo; i++)
		{
			fprintf (stderr, "U%d= %f\n",i, dPeriodicUtil[i]);
		}
		ch = getchar();
		return;
	}
	
/****************************************************************************************************************/
/* Implementing of RUN scheduling                                                                               */
/****************************************************************************************************************/
#if RUN == 1
#if debug == 1
	printf ( "Scheduling using RUN...\n" );
#endif
	Initialize();
	int iHasNewTask = 0;
	int iHasNewInstance = 0;
	TCB *p;
	for (ullTick = 0; ullTick < TICKS;) 
	{
		uiOverhead = 0;
		for (i = 0; i < MAX_TASKS; i++) 
		{
			if (ullRequestTime[i][uiInstanceNo[i]] == ullTick) 
			{
				pTCBEntry = entry_set(i);
				if (iIsPeriodic[i] == 1) 
				{
					pTCBEntry -> ullAbsoluteDealine = ullTick + ullPeriod[i];
					pTCBEntry -> uiET = uiWCET[i];
					fnRunInsertWaitingQueue(&pTCBWaitingQueue, pTCBEntry);
					if(ullPhase[i] == ullTick)
					{
						iHasNewTask = 1;
					}
				}
				else
				{
					uiAInstanceNo ++;
					fnRunInsertAperiodic(pTCBEntry);
				}
				uiInstanceNo[i]++;
				uiTotalInsNo++;
				// Re-build the reduction tree when a new task arrives
				
				iHasNewInstance = 1;
			}		  
		}
		if(iHasNewTask)
		{
			//fprintf(stderr, "Current task %d, time %d, instance %d\n", i, ullTick, uiInstanceNo[i]);
			iHasNewTask = 0;
			fnBuildReductionTree(&pTopSer, &pTCBWaitingQueue);
			if(iHasNewInstance) iHasNewInstance = 0;
			
			// Involve the RUN scheduling
			fnSearchRUN(&pTopSer);
			uiSchedulerCout ++;
		}
		else if(iHasNewInstance)
			{
				//fprintf(stderr,"New instance\n");
				iHasNewInstance = 0;
				fnUpdateReductionTree(&pTopSer);
				fnSearchRUN(&pTopSer);
				uiSchedulerCout ++;
			}
			else if(iHasServerFinished || iHasJobFinished)
				{
					//Check whether there is any server finished or job completed, if any then involving the scheduler
					iHasServerFinished = 0;
					iHasJobFinished = 0;
					fnSearchRUN(&pTopSer);
					uiSchedulerCout ++;
				}

		// Scheduling
		fnRunScheduling();
		
		//fnEvaluation();
		
		// Tick increment/ et decrement
		fnRunTickInc();
		
		//Update ET for selected servers
		fnRunTickUpdateSelectedServers(pSelectedServer);
		
		unsigned int uiPID;
		for(uiPID = 0; uiPID < MAX_PRO; uiPID++)
		{
			if ( pTCBKernelRuntsk[uiPID] != NULL ) 
			{
				if ( pTCBKernelRuntsk[uiPID] -> uiET == 0) 
				{
					p = pTCBKernelRuntsk[uiPID];
					pTCBKernelRuntsk[uiPID] == NULL;
					fnRunJobComplete("RUN_Execution", p);
					pTCBKernelRuntskPre[uiPID] = NULL;
				}
			} 
		}
	}
  //fprintf(stderr,"Instance No.: %d\n", uiTotalInsNo);
  printf(", %d\n", uiSchedulerCout);
  
  fprintf ( migration_fp, ", %d\n", uiMigrationCout );
  fprintf ( preemption_fp, ", %d\n", uiPreemptionCout );
  fprintf ( overhead_fp, ", %d\n", uiOverhead );
  dAverageResponse = uiAResponseSum / uiAInstanceNo;
	fprintf ( response_fp, ", %f\n", dAverageResponse );
	fprintf (stderr, "Aperiodic summary: No. instance = %d, Finished No = %d, average response = %0.2lf\n", uiAInstanceNo, apFinishedNo, dAverageResponse);

#else
  printf ( "No Scheduling RUN\n" );

  fprintf ( migration_fp, ",\n" );
  fprintf ( preemption_fp, ",\n" );
  fprintf ( overhead_fp, ",\n" );
  fprintf ( response_fp, ",\n" );

#endif
/********************************************************************************************************************************************/ 
  fclose ( migration_fp );
  fclose ( preemption_fp );
  fclose ( overhead_fp );

	return 0;
}

/****************************************************************************************************************/
/* RUN: Function implementation                                                                               */
/****************************************************************************************************************/

void Initialize(void)
{
#if debug == 1
	fprintf(stderr, "Initializing system...\n");
#endif
	// Variables in common use
	int i, j;
	pTCBWaitingQueue  	= NULL;
	pAWQ 	= NULL;
	pARQ	= NULL;
	for ( i = 0; i < MAX_PRO; i++ ) 
	{
		pTCBKernelRuntsk[i] 	= NULL;
		pTCBKernelRuntskPre[i] 	= NULL;
	}
	
	for(i = 0; i < MAX_TASKS; i++)
	{
		uiPreAssign[i] 		= MAX_PRO;
		uiLastAssign[i] 	= MAX_PRO;
		iPrevScheduled[i] 	= -1;
	}
	//uiPTaskNo = 0;
	uiSchedulerCout 	= 0;
	uiPreemptionCout	= 0;
	uiMigrationCout 	= 0;
	uiOverhead 			= 0;
	uiOverheadMax 		= 0;
	uiTotalInsNo 		= 0;
	uiAResponseSum 		= 0;
	dAverageResponse 	= 0;
	uiAInstanceNo 		= 0;
	apFinishedNo		 = 0;

	
	// Variables for RUN
#if RUN == 1
	pTopSer = NULL; 
	pSelectedServer = NULL;
	iHasServerFinished = 0;
	iHasJobFinished = 0;
	for ( i = 0; i < MAX_PRO; i++ ) 
	{
		pSCBReadyQueue[i]	= NULL;
		uiAssigned[i] 		= 0;
	}
	uiSelectedNo = 0;

#endif
  return;
}

void	fnRunScheduling(void)
{
#if debug == 1
	fprintf( stderr,"Scheduling...\n");
#endif
	int i,j;
	for(i = 0; i < MAX_TASKS; i++)
	{
		iPrevScheduled[i] = -1;
	}
	// scheduling periodic tasks first
	for (i = 0; i < MAX_PRO; i++)
	{
		pTCBKernelRuntsk[i] = pTCBReadyQueue[i];
		if(pTCBKernelRuntsk[i] != NULL)
		{
			if(uiLastAssign[pTCBKernelRuntsk[i] -> iTaskID] != i)
			{
				uiLastAssign[pTCBKernelRuntsk[i] -> iTaskID] = i;
				uiMigrationCout ++;
			}
			iPrevScheduled[pTCBKernelRuntsk[i] -> iTaskID] = i;
		}
			
	}
	
	// scheduling aperiodic tasks in background
	TCB *p;
	p = pARQ;
	
	for(i = 0; i < MAX_PRO; i++)
	{
		if(pTCBKernelRuntsk[i] == NULL)
		{
			if(p != NULL)
			{
				//fprintf( stderr,"Execute aperiodic task %d...\n", p -> iTaskID);
				pTCBKernelRuntsk[i] = p;
				p = p -> pNext;
			}
			else
			{
				break;
			}
		}
	}
	return;
}

void	fnRunTickInc(void)
{
	#if debug == 1
	fprintf( stderr,"Tick increasing...\n");
#endif	
	ullTick++;
#if debug == 1
	fprintf( stderr,"Tasks executing...\n");
#endif	
	unsigned int pid;
	for(pid = 0; pid < MAX_PRO; pid++)
	{
		if ( pTCBKernelRuntsk[pid] != NULL ) 
		{
			--(pTCBKernelRuntsk[pid] -> uiET);
		} 
	}
	return;
}

void	fnRunJobComplete(const char *s, TCB *tcb)
{
#if debug == 1
	fprintf( stderr,"RUN: A job of Task %d finished.\n", pTCBKernelRuntsk[uiProcID]->iTaskID);
#endif

	unsigned int response = ullTick - tcb -> ullRequestTime;
	if ( tcb -> ullAbsoluteDealine < ullTick ) 
	{
		fprintf( stderr, "\n# MISS: %s: ullTick=%lld: DL=%lld: TID=%d: INST_NO=%d, ullRequestTime=%lld\n",
			s, ullTick, tcb -> ullAbsoluteDealine, tcb -> iTaskID, tcb -> iInstanceNo, tcb -> ullRequestTime);
		printf( "\n# MISS: %s: ullTick=%lld: DL=%lld: TID=%d: INST_NO=%d, ullRequestTime=%lld\n",
			s, ullTick, tcb->ullAbsoluteDealine, tcb->iTaskID, tcb->iInstanceNo, tcb->ullRequestTime);
	}
	iHasJobFinished = 1;
	uiOverhead += ASSIGN;
	
	if(iIsPeriodic[tcb -> iTaskID] == 0)
	{
		//fprintf(stderr, "Tick %d: Aperiodic task finished %d, response = %d \n", ullTick, tcb -> iTaskID, response);
		apFinishedNo ++;
		uiAResponseSum += response;
		fnRunJobRemove(tcb, 0);
	}
	return;
}

void	fnRunJobRemove(TCB *tcb, int periodic)
{
	//fprintf( stderr,"Removing task %d\n", tcb -> iTaskID);
	unsigned int tid = tcb -> iTaskID;
	if(periodic == 1)
	{
		return;
	}
	else
	{
		if(tcb == pARQ)
			pARQ = tcb -> pNext;
	}
	if(tcb -> pPrev != NULL)
	{
		tcb -> pPrev -> pNext = tcb -> pNext;
	}
	if(tcb -> pNext != NULL)
	{
		tcb -> pNext -> pPrev = tcb -> pPrev;
	}
	//fprintf( stderr,"Finished removing task %d\n", tcb -> iTaskID);
	free(tcb);
	
	return;
}

TCB *entry_set( int i )
{
#if debug == 1
	fprintf( stderr,"Creating a new TCB ID = %d...\n", uiTaskID);
#endif
  TCB *pTCBEntry;

  pTCBEntry = malloc ( sizeof ( TCB ) );
  pTCBEntry -> iTaskID				= i;
  pTCBEntry -> iInstanceNo			= uiInstanceNo[i];
  pTCBEntry -> ullRequestTime		= ullTick;
  pTCBEntry -> uiET					= uiExeTime[i][uiInstanceNo[i]];
  //pTCBEntry -> uiET				= uiWCET[i];
  pTCBEntry -> pNext 				= pTCBEntry -> pPrev 	= NULL;

  pTCBEntry -> ullAbsoluteDealine 	= (unsigned long long)0xFFFFFFFFFFFFFFFF;  // dummy necessary

  return pTCBEntry;

}

TID	*tid_set( int tid)
{
#if debug == 1
	fprintf(stderr,"Creating a new TID %d...\n", tid);
#endif
	TID	*pTIDEntry;
	
	pTIDEntry = malloc ( sizeof ( TID ) );
	pTIDEntry -> iTaskID             = tid;
	pTIDEntry -> pNext = pTIDEntry -> pPrev = NULL;

	return pTIDEntry;
	
}

TCB		*fnSeekTCB(TCB **wq, unsigned int uiTaskID)
{
#if debug == 1
	fprintf(stderr,"Seeking TID %d...\n", uiTaskID);
#endif
	TCB	*pTCB;
	uiOverhead += ASSIGN + COMP;
	for(pTCB = *wq; pTCB != NULL; pTCB = pTCB -> pNext)
	{
		uiOverhead += COMP;
		if (pTCB -> iTaskID == uiTaskID)
			break;
		
		uiOverhead += MEM + COMP;
	}
#if debug == 1
	fprintf(stderr,"Finished seeking TID %d.\n", uiTaskID);
#endif	
	return pTCB;
}

void	fnRunInsertWaitingQueue(TCB **wq, TCB *pTCBEntry)
{
#if debug == 1
	fprintf( stderr,"RUN: Inserting new TCB to the waiting queue...\n");
#endif
	TCB  *p;
	if (*wq == NULL) 
	{
		*wq = pTCBEntry;
		return;
	}
	
	for(p = (*wq); p != NULL; p = p -> pNext)
	{
		if(p -> iTaskID == pTCBEntry -> iTaskID)
			break;
	}
	
	if(p != NULL)
	{
		p -> iInstanceNo = pTCBEntry -> iInstanceNo;
		p -> ullRequestTime = pTCBEntry -> ullRequestTime;
		p -> ullAbsoluteDealine = pTCBEntry -> ullAbsoluteDealine;
		p -> uiET = pTCBEntry -> uiET;
	}
	else
	{
		for(p = (*wq); p -> pNext != NULL; p = p -> pNext);
		p -> pNext = pTCBEntry;
		pTCBEntry -> pPrev = p;
	}
	return;
}

void	fnRunInsertAperiodic(TCB *tcb)
{
	if(fnRunJobToRQ(&pARQ, tcb) == 1)
		return;
	else
		return fnRunJobToWQ(&pAWQ, tcb);
}

int		fnRunJobToRQ(TCB **aq, TCB *tcb)
{
	if((*aq) == NULL)
	{
		(*aq) = tcb;
		return 1;
	}
	
	TCB *p;
	for (p = *aq; p != NULL; p = p -> pNext)
	{
		if (p -> iTaskID == tcb -> iTaskID)
			break;
	}
	if(p != NULL) return 0;
	
	// insert new aperiodic job to ready queue in the ascending order of release time
	for (p = *aq; p -> pNext != NULL; p = p -> pNext);
	
	p -> pNext = tcb;
	tcb -> pPrev = p;
	
	return 1;
}

void	fnRunJobToWQ(TCB **aq, TCB *tcb)
{
	if((*aq) == NULL)
	{
		(*aq) = tcb;
		return;
	}
	TCB *p;
	for (p = *aq; p -> pNext != NULL; p = p -> pNext);
	
	p = p -> pNext = tcb;
	tcb -> pPrev = p;
	
	return;
}

void   fnBuildReductionTree(SCB **topRoot, TCB **wq)
{
#if debug == 1
	fprintf(stderr, "Tick %d: Building servers from the existing tasks...\n", ullTick);
#endif
	SCB *pPackedList = NULL;
	SCB *pServerList = NULL;
	//Todo: Free the current reduction tree, if any, before building a new one	

	// Build a server list from waiting queue of TCB
	pServerList = fnBuildServers(wq);
#if debug == 1
	fnFprintServerList(pServerList, "Generated servers");
	fprintf(stderr, "Rebuilding reduction tree...\n");
#endif
	while(1)
	{
		// Packing
		pPackedList = fnPack(&pServerList);
#if debug == 1
		fnFprintServerList(pPackedList, "Packed servers");
		getchar();
#endif
		// Remove proper subsystems from packed list
		fnRemoveProperSubsystem(topRoot, &pPackedList);
#if debug == 1
		fnFprintServerList(pPackedList,"Current improper servers");
		getchar();
#endif		
		// Check whether all proper subsystems are obtained
		if(pPackedList == NULL) break;
		
		// Dual
		pServerList = fnDual(&pPackedList);
#if debug == 1
		fnFprintServerList(pServerList, "Dualed servers");
		getchar();
#endif
		}
#if debug == 1
	fprintf(stderr, "Building reduction tree is done.\n");
	fnFprintServerList(*topRoot, "Reduction trees");
	getchar();
#endif
	
	return;
}

SCB		*fnBuildServers(TCB **wq)
{
#if debug == 1
	fprintf(stderr, "Creating servers from existing TCB...\n");
#endif
	TCB *pTCB;
	SCB *pSCBList = NULL;
	SCB *pNewEntry;
	
	// Create servers for existing tasks
	for(pTCB = *wq; pTCB != NULL; pTCB = pTCB -> pNext)
	{
		pNewEntry = fnSCBByTCB(pTCB);
		fnSCB2List(&pSCBList, pNewEntry);
	}
	
	// Create dummy tasks, if any double dDumUltil
#if debug == 1
	fprintf(stderr, "Adding dummy servers...\n");
#endif
	double dDumUltil;
	for(dDumUltil = MAX_PRO - dPeriodicUtilSum; dDumUltil > 0; dDumUltil--)
	{		
		if(dDumUltil <= 1)
		{
			pNewEntry = fnSCBDummy(round(dDumUltil));
			fnSCB2List(&pSCBList, pNewEntry);
			break;
		}
		else
		{
			pNewEntry = fnSCBDummy(1);
			fnSCB2List(&pSCBList, pNewEntry);
		}
	}
	
	return pSCBList;
}

SCB		*fnSCBByTCB(TCB *entry)
{
#if debug == 1
	fprintf(stderr, "Creating a new SCB from a TCB...\n");
#endif
	SCB	*pNewSCB;
	pNewSCB = malloc(sizeof(SCB));
	
	pNewSCB -> uiLeafNo 	= 0;
	pNewSCB -> uiIsPacked 		= 1;
	pNewSCB -> dUtilization = dPeriodicUtil[entry -> iTaskID];
	pNewSCB -> ullADL = entry -> ullAbsoluteDealine;
	pNewSCB -> uiET		= pNewSCB -> dUtilization * ullPeriod[entry -> iTaskID];	
	pNewSCB -> pTID = tid_set(entry -> iTaskID);
	pNewSCB -> pLeaf = NULL;
	pNewSCB -> pRoot = NULL;
	pNewSCB -> pNext = NULL;
	pNewSCB -> pPrev = NULL;
	
	return pNewSCB;
}

SCB		*fnSCBDummy(double ultil)
{
#if debug == 1
	fprintf(stderr, "Creating a new dummy server...\n");
#endif
	SCB	*pNewSCB;
	pNewSCB = malloc(sizeof(SCB));
	
	pNewSCB -> uiLeafNo 	= 0;
	pNewSCB -> uiIsPacked 		= 1;
	pNewSCB -> uiET		= (unsigned int)0xFFFFFFFF;  // dummy necessary
	pNewSCB -> dUtilization = round(ultil);
	pNewSCB -> ullADL = (unsigned long long)0xFFFFFFFFFFFFFFFF;
	pNewSCB -> pTID = tid_set(MAX_TASKS);
	pNewSCB -> pLeaf = NULL;
	pNewSCB -> pRoot = NULL;
	pNewSCB -> pNext = NULL;
	pNewSCB -> pPrev = NULL;
	
	return pNewSCB;
}

void	fnSCB2List(SCB **scbq, SCB *pSCBEntry)
{
#if debug == 1
	fprintf(stderr, "Adding an SCB to a server list\n");
#endif
	SCB	*p;
	//uiOverhead += COMP;
	if(*scbq == NULL)
	{
		(*scbq) = pSCBEntry;
		//uiOverhead += ASSIGN;
		return;
	}
	//uiOverhead += ASSIGN + COMP;
	for (p = (*scbq); p -> pNext != NULL; p = p -> pNext)
	{
		//uiOverhead += MEM + COMP;
	}
	p -> pNext = pSCBEntry;
	pSCBEntry -> pPrev = p;
	//uiOverhead += ASSIGN + ASSIGN;
	return;
}

SCB	*fnGetSCB(SCB **scbq)
{
#if debug ==1
	fprintf(stderr, "Getting an SCB from server list...\n");
#endif
	SCB *pSCB;
	pSCB = (*scbq);
	if(pSCB -> pNext != NULL)
	{
		(*scbq) = pSCB -> pNext;
		(*scbq) -> pPrev = NULL;
		pSCB -> pNext = NULL;
	}
	else
	{
		(*scbq) = NULL;
	}
	return pSCB;
}

void	fnSCB2Pack(SCB **root, SCB *leaf)
{
#if debug == 1 
	fprintf(stderr, "Adding an SCB to a server pack...\n");
#endif
	int i;
	(*root) -> uiLeafNo += 1;
	(*root) -> uiIsPacked 		= 1;
	
	(*root) -> dUtilization = round((*root) -> dUtilization) + round(leaf -> dUtilization);
	//uiOverhead += ASSIGN + FADD;
	fnAddTID(&((*root) -> pTID), leaf -> pTID);
	//uiOverhead += COMP;
	if((*root) -> pLeaf == NULL)
	{
		(*root) -> pLeaf = leaf;
		//uiOverhead += ASSIGN;
	}
	else
	{
		SCB *p;
		//uiOverhead += ASSIGN + COMP;
		for(p = (*root) -> pLeaf; p -> pNext != NULL; p = p -> pNext)
		{
			//uiOverhead += MEM + COMP;
		}
		p -> pNext = leaf;
		leaf -> pPrev = p;
		//uiOverhead += ASSIGN + ASSIGN;
	}
	(*root) -> ullADL = (*root) -> pLeaf -> ullADL;
	(*root) -> uiET		= (*root) -> dUtilization * (*root) -> ullADL;
	
	leaf -> pRoot = (*root);
	//uiOverhead += ASSIGN + ASSIGN + FMUL + ASSIGN;
	return;
}

void	fnAddTID(TID **tidq, TID *pTIDEntry)
{
#if debug == 1
	fprintf(stderr, "Adding a new TID to task list of server...\n");
#endif
	TID	*pTID;
	//uiOverhead += COMP;
	if(*tidq == NULL)
	{
		(*tidq) = pTIDEntry;
		//uiOverhead += ASSIGN;
		return;
	}
	//uiOverhead += ASSIGN + COMP;
	for (pTID = *tidq; pTID -> pNext != NULL; pTID = pTID -> pNext)
	{
		//uiOverhead += MEM + COMP;
	}
	pTID -> pNext = pTIDEntry;
	pTIDEntry -> pPrev = pTID;
	//uiOverhead += ASSIGN + ASSIGN;
	return;
}

int 	get_tid_from_server(SCB *scb)
{
	return scb -> pTID -> iTaskID;
}

SCB	*fnPack(SCB **SCBList)
{
#if debug == 1
	fprintf(stderr, "Packing servers...\n");
#endif	
	SCB	*pEntry;
	SCB *pPacked = NULL;
	SCB *pEmptiest;
	SCB *pNewBin;
	SCB *p;
	double Ultil = 1;
	
	/*For the first server*/
	// Create the first bin
	pNewBin = malloc(sizeof(SCB));
	
	// Add the new bin to the packed list
	fnSCB2List(&pPacked, pNewBin);
	
	// Set the new bin as the emptiest
	pEmptiest = pNewBin;
	//uiOverhead += ASSIGN;
#if debug == 1
	fnFprintServerList(*SCBList);
#endif

	// Get the first server from the server list
#if debug == 1
	fprintf(stderr, "Pack the first server\n");
#endif
	pEntry = fnGetSCB(SCBList);

	// Add the first server to bin
	fnSCB2Pack(&pEmptiest, pEntry);
	
	/*For the other servers*/
	while((*SCBList) != NULL)
	{
#if debug ==1 
		fnFprintServerList(*SCBList);
		fprintf(stderr, "Pack the other server\n");
#endif
		// get the next server
		pEntry = fnGetSCB(SCBList);
		
		// Add server to bins
		if((pEntry -> dUtilization) <= round(1.0 - pEmptiest -> dUtilization))
		{
			//Add to the emptiest bin
			fnSCB2Pack(&pEmptiest, pEntry);
			
			//Update pEmptiest
			for(p = pPacked; p != NULL; p = p -> pNext)
			{
				if(pEmptiest -> dUtilization > p -> dUtilization)
				{
					pEmptiest = p;
				}
			}
		}
		else
		{
			//Create a new bin to add the server
			pNewBin = malloc(sizeof(SCB));
			fnSCB2List(&pPacked, pNewBin);
			fnSCB2Pack(&pNewBin, pEntry);
			
			//Update the pEmptiest
			if(pEmptiest -> dUtilization > pNewBin -> dUtilization)
			{
				pEmptiest = pNewBin;
			}
		}
	}
	return pPacked;
}

SCB		*fnDual(SCB **serverList)
{
#if debug == 1
	fprintf(stderr, "Dualing servers...\n");
#endif
	SCB	*pDualList;
	SCB *pSCB;
	pDualList = NULL;
	//uiOverhead += ASSIGN + COMP;
	for(pSCB = *serverList; pSCB != NULL; pSCB = pSCB -> pNext)
	{
		fnSCB2List(&(pDualList), fnDualSet(pSCB));
		//uiOverhead += MEM + COMP;
	}
	return pDualList;
}

SCB		*fnDualSet(SCB *Entry)
{
#if debug ==1
	fprintf(stderr, "Creating a dual server...\n");
#endif
	SCB	*pDualSCB;
	pDualSCB = malloc(sizeof(SCB));
	pDualSCB -> uiLeafNo = 1;
	pDualSCB -> uiIsPacked = 0;		// 0: by Dual; 1: by Packing
	pDualSCB -> dUtilization = round(1.0 - Entry -> dUtilization);
	pDualSCB -> pTID = tid_set(Entry -> pTID -> iTaskID);
	pDualSCB -> pLeaf = Entry;
	pDualSCB -> ullADL = Entry -> ullADL;
	pDualSCB -> uiET = pDualSCB -> dUtilization * pDualSCB -> ullADL;
	pDualSCB -> pRoot = NULL;
	pDualSCB -> pNext = NULL;
	pDualSCB -> pPrev = NULL;
	
	Entry -> pRoot = pDualSCB;
	
	return pDualSCB;
}

void	fnRemoveProperSubsystem(SCB **topRoot, SCB **packedList)
{
#if debug == 1
	fprintf(stderr, "Moving proper subsystems...\n");
#endif
	SCB *p;
	SCB *proper;
	for(p = (*packedList); p != NULL;)
	{
		if(p -> dUtilization == 1.0)
		{
			//slit p from packedList and add to topRoot
			proper = p;
			p = p -> pNext;
			fnSlitSCB(packedList, proper);
			fnSCB2List(topRoot, proper);
		}
		else
		{
			p = p -> pNext;
		}
	}
	return;
}

void	fnSlitSCB(SCB **scbq, SCB *slitted)
{
#if debug == 1
	fprintf(stderr, "Slitting an SCB from server list...\n");
#endif
	if(slitted == (*scbq))
	{
		(*scbq) = (*scbq) -> pNext;
		if((*scbq) != NULL)
			(*scbq) -> pPrev = NULL;
	
		slitted -> pPrev = slitted -> pNext = NULL;
	}
	else
	{
		slitted -> pPrev -> pNext = slitted -> pNext;
		if(slitted -> pNext != NULL)
			slitted -> pNext -> pPrev = slitted -> pPrev;
		slitted -> pPrev = slitted -> pNext = NULL;
	}
	return;
}

void	fnFreeServerList(SERLIST **list)
{
	while(*list)
	{
		SERLIST *p = *list;
		*list = (*list) -> pNext;
		if(*list)
			(*list) -> pPrev = NULL;
		free(p);
	}
	*list = NULL;
}

void	fnSearchRUN(SCB **root)
{
#if debug == 1
	fprinf(stderr, "Searching servers on the reduction tree...\n");
#endif
	//Reset the selected server list
#if debug == 1
	fprintf(stderr, "Resetting selected server list...\n");
#endif
	fnFreeServerList(&pSelectedServer);
	uiSelectedNo = 0;
#if debug == 1	
	fprintf(stderr, "Start searching servers...\n");
#endif

	SCB *p;
	int iSelected = 1; // the root of proper subsystem is always selected

	for(p = (*root); p != NULL; p = p -> pNext)
	{
		fnSearchNode(p, iSelected);
	}
#if debug == 1	
	fprintf(stderr, "Searching is done.\n");
#endif
	//assign corresponding tasks of selected servers to appropriate processors
	fnRUNAssign();
	
#if db_MEM == 1
	for testing only
	fprintf(stderr, "Tick %d: Result assign pid(tid): \n", ullTick);
	int i;
	for(i = 0; i < MAX_PRO; i++)
	{
		if(pTCBReadyQueue[i] != NULL)
		{
			fprintf(stderr, "%d(%d) ", i, pTCBReadyQueue[i] -> iTaskID);
		}
		
	}
	fprintf(stderr, "\n");
	getchar();
#endif
#if debug == 1
	fnFprintTCBReady("Processor Assignment");
#endif	
	return;
}

void	fnSearchNode(SCB *searchNode, int iSelected)
{
#if debug == 1
	fprinf(stderr, "Search servers on a node...\n");
#endif
	uiOverhead += COMP;
	if(searchNode -> uiIsPacked == 1)
	{
		uiOverhead += COMP;
		if(searchNode -> pLeaf != NULL)
		{
			uiOverhead += COMP;
			if(iSelected) // pack is selected
			{
				// Using EDF to select a server in the pack
				SCB *p, *q;
				uiOverhead += ASSIGN + COMP;
				for(q = searchNode -> pLeaf; q != NULL; q = q -> pNext)
				{
					uiOverhead += COMP;
					if(q -> uiET == 0)
						fnSearchNode(q, !iSelected);
					else
						break;
					uiOverhead += MEM + COMP;
				}
				uiOverhead += COMP;
				if(q != NULL)
				{
					uiOverhead += ASSIGN + COMP;
					for(p = q -> pNext; p != NULL; p = p -> pNext)
					{
						uiOverhead += COMP + COMP;
						if((p -> ullADL < q -> ullADL) && (p -> uiET != 0))
						{
							fnSearchNode(q, !iSelected);
							q = p;
							uiOverhead += ASSIGN;
						}
						else
						{
							fnSearchNode(p, !iSelected);
						}
						uiOverhead += MEM + COMP;
					}
					fnAdd2SelectedServers(q);
					fnSearchNode(q, iSelected);
				}
			}
			else // pack is not selected
			{
				SCB *p;
				uiOverhead += ASSIGN + COMP;
				for(p = searchNode -> pLeaf; p != NULL; p = p -> pNext)
				{
					fnSearchNode(p, iSelected);
					uiOverhead += MEM + COMP;
				}
			}
		}
		else
		{
			uiOverhead += COMP;
			if(iSelected)
			{
				fnAdd2ReadyServers(searchNode);
#if debug == 1
				fnFprintServer(searchNode, "Selected Server");
#endif
				return;
			}
			else
			{
				return;
			}
		}
	}
	else
	{
		SCB *p;
		p = searchNode -> pLeaf;
		uiOverhead += ASSIGN;
		return fnSearchNode(p, !iSelected);
	}
}

void	fnAdd2SelectedServers(SCB *selected)
{
#if debug == 1
	fnFprintServer(selected, "Selected servers");
#endif
	SERLIST *p = malloc(sizeof(SERLIST));
	p -> scb = selected;
	p -> pNext = p -> pPrev = NULL;
	uiOverhead += ASSIGN + ASSIGN + ASSIGN;
	uiOverhead += COMP;
	if(pSelectedServer == NULL)
	{
		pSelectedServer = p;
		uiOverhead += ASSIGN;
		return;
	}
	else
	{
		SERLIST *q;
		uiOverhead += ASSIGN + COMP;
		for(q = pSelectedServer; q -> pNext != NULL; q = q -> pNext)
		{
			uiOverhead += MEM + COMP;
		}
		q -> pNext = p;
		p -> pPrev = q;
		uiOverhead += ASSIGN + ASSIGN;
	}
	return;
}

void 	fnAdd2ReadyServers(SCB *selected)
{
#if debug == 1
	fprinf(stderr, "Add a node as a ready server...\n");
#endif
	pSCBReadyQueue[uiSelectedNo] = selected;
	uiSelectedNo++;
	uiOverhead += ASSIGN + IADD;
	return;
}

void	fnRUNAssign()
{
#if debug == 1
	fprintf(stderr, "Assigning tasks to processors...\n");
#endif
	int i, j;
	int tid;
	
	//Resetting the processors
	for(i = 0; i < MAX_PRO; i++)
	{
		uiAssigned[i] = 0;
		pTCBReadyQueue[i] = NULL;
	}
	
	//attempt to assign tasks to their last processor
	  for(i = 0; i< uiSelectedNo; i++)
	{
		tid = get_tid_from_server(pSCBReadyQueue[i]);

		if(uiLastAssign[tid] < MAX_PRO && uiAssigned[uiLastAssign[tid]] != 1)
		{
			pTCBReadyQueue[uiLastAssign[tid]] = fnSeekTCB(&pTCBWaitingQueue, tid);
			uiAssigned[uiLastAssign[tid]] = 1;
			pSCBReadyQueue[i] = NULL;
		}
	} 

	//assign the others selected tasks to the other processors arbitrarily
	for( i = 0, j = 0; i < uiSelectedNo && j < MAX_PRO; )
	{
		if(pSCBReadyQueue[i] == NULL) 			//the selected task is assigned to processor already
		{
			i++;								//continuing the next one
		}
		else 									//assigning the selected task to a appropriate processor
		{
			if(uiAssigned[j] == 1)				//the current processor is assigned task already
			{
				j++; 							//continuing the next processor
			}
			else 								//the current processor is available
			{
				//Assigning the selected task to the current processor
				tid = get_tid_from_server(pSCBReadyQueue[i]);
				pTCBReadyQueue[j] = fnSeekTCB(&pTCBWaitingQueue, tid);
				uiAssigned[j] = 1;
				pSCBReadyQueue[i] = NULL;
				j++; 							//continuing the next processor
				i++; 							//continuing the next selected task
			}
		}
	}
	
	
#if debug == 1
	fprintf(stderr, "Assigning is done. \n");
#endif	
}

void	fnUpdateReductionTree(SCB **root)
{
#if debug == 1
	fprinf(stderr, "Updating reduction tree...\n");
#endif
	SCB *p;
	uiOverhead += ASSIGN + COMP;
	for(p = (*root); p != NULL; p = p -> pNext)
	{
		fnUpdateNode(p);
		uiOverhead += MEM + COMP;
	}
	return;
}
void	fnUpdateNode(SCB *Node)
{
#if debug == 1
	fprinf(stderr, "Updating a node of server...\n");
#endif
	uiOverhead += COMP;
	if(Node -> uiIsPacked == 1) // update packed node
	{
		uiOverhead += COMP;
		if(Node -> pLeaf != NULL)
		{
			SCB *p;
			// Update leaf servers
			uiOverhead += ASSIGN + COMP;
			for(p = Node -> pLeaf; p != NULL; p = p -> pNext)
			{
				fnUpdateNode(p);
				uiOverhead += MEM + COMP;
			}
			//Update the root server
			SCB *q = Node -> pLeaf;
			uiOverhead += ASSIGN;
			uiOverhead += ASSIGN + COMP;
			for(p = Node -> pLeaf -> pNext; p != NULL; p = p -> pNext)
			{
				uiOverhead += COMP;
				if(p -> ullADL < q -> ullADL)
				{
					q = p;
					uiOverhead += ASSIGN;
				}
				uiOverhead += MEM + COMP;	
			}
			Node -> ullADL = q -> ullADL;
			Node -> uiET = q -> uiET;
			uiOverhead += ASSIGN + ASSIGN;
		}
		else
		{
			uiOverhead += COMP;
			if(Node -> uiET == 0)
			{
				TCB *q;
				q = fnSeekTCB(&pTCBWaitingQueue, get_tid_from_server(Node));
				Node -> uiET = q -> uiET;
				Node -> ullADL = q -> ullAbsoluteDealine;
			}
		}
		
		
	}
	else // Update dual node
	{
		// update leaf
		fnUpdateNode(Node -> pLeaf);
		
		// update node
		Node -> ullADL = Node -> pLeaf -> ullADL;
		Node -> uiET = Node -> dUtilization * Node -> ullADL;
	}
}

void	fnRunTickUpdateSelectedServers(SERLIST *selected)
{
#if debug == 1
	fprintf(stderr, "Updating selected servers by tick...\n");
#endif
	SERLIST *p;
	uiOverhead += ASSIGN + COMP;
	for(p = selected; p != NULL; p = p -> pNext)
	{
		p -> scb -> uiET--;
		uiOverhead += IADD;
		uiOverhead += COMP;
		if(p -> scb -> uiET == 0)
		{
			iHasServerFinished = 1;
			uiOverhead += IADD;
		}
			
	}
	
}

void Record(void)
{
#if debug == 1
	fprintf( stderr,"Saving evaluation results...\n");
#endif
	int i;
	int iTotalIns = 0;
	for (i = 0; i < MAX_TASKS; i++)
	{
		iTotalIns += uiInstanceNo[i]; 
	}
	//fprintf();
	printf("Instance number:, %d\n", iTotalIns);
	printf("Scheduler count:, %d\n", uiSchedulerCout);
	printf("Preemption count:, %d\n", uiPreemptionCout);
	printf("Migration count:, %d\n", uiMigrationCout);
	return;
}

void	fnFprintServerList(SCB *serverList, const char *s)
{
	fprintf(stderr, "%s (tid, PoD, util, et, adl): ", s);	
	SCB *p;	
	for(p = serverList; p != NULL; p = p -> pNext)
	{
		if(p != NULL)
			fprintf(stderr, "(%d, %d, %.2lf, %d, %d)  ", p -> pTID -> iTaskID, p -> uiIsPacked, round(p -> dUtilization), p -> uiET, p -> ullADL);
		else
			fprintf(stderr, "Error: Server list is empty.\n"); 
	}
	fprintf(stderr, "\n");
}

void	fnFprintServer(SCB *server, const char *s)
{
	fprintf(stderr, "%s (tid, PoD, util, adl): ", s);
	if(server)
	{
		fprintf(stderr, "(%d, %d, %.2lf, %d, %d)\n", server -> pTID -> iTaskID, server -> uiIsPacked, 
						round(server -> dUtilization), server -> uiET, server -> ullADL);
	}
	else
	{
		fprintf(stderr, "Error: No servers.\n");
	}
	
}

void	fnFprintTCBReady(const char *s)
{
	int i;
	fprintf(stderr, "%s PID(TID): ", s);
	for(i = 0; i < MAX_PRO; i++)
	{
		if(pTCBReadyQueue[i] != NULL)
			fprintf(stderr, "%d(%d) ", i, pTCBReadyQueue[i] -> iTaskID);
		else
			fprintf(stderr, "%d(NULL) ", i);
	}
	fprintf(stderr, "\n");
}

double	round(double num)
{
	int inum = (int)(num * 10);
	double dnum = inum + 0.5;
	if(num * 10 >= dnum)
		return ceil(num*10)/10.0;
	return floor(num*10)/10.0;
}

void	fnEvaluation()
{
	int i, j;
	//Counting the number of preemptions
	for (i = 0; i < MAX_PRO; i++)
	{
		if(pTCBKernelRuntskPre[i] != NULL)
		{
			for(j = 0; j < MAX_PRO; j++)
			{
				if(pTCBKernelRuntsk[j] != NULL)
				{
					if(pTCBKernelRuntskPre[i] -> iTaskID == pTCBKernelRuntsk[j] -> iTaskID) break;
				}
			}
			if(j == MAX_PRO)
			{
				//fprintf(stderr, "Tick %d Previous empty %d\n", ullTick, pTCBKernelRuntskPre[i] -> iTaskID);
				if(pTCBKernelRuntskPre[i] -> uiET < uiWCET[pTCBKernelRuntskPre[i] -> iTaskID])
				{
					uiPreemptionCout++;
				}
			}
		}
		//else{
		//	fprintf(stderr, "Tick %d Previous empty \n", ullTick);
		//}
		
		pTCBKernelRuntskPre[i] = pTCBKernelRuntsk[i];
	}
	
	//Counting the number of task migrations
	unsigned int iTaskID;
	for(i = 0; i < MAX_PRO; i++)
	{
		if(pTCBKernelRuntsk[i] != NULL)
		{
			iTaskID = pTCBKernelRuntsk[i] -> iTaskID;
			if(uiPreAssign[iTaskID] != i)
			{
				if(uiPreAssign[iTaskID] != MAX_PRO)
				{
					uiMigrationCout ++;
				}
				uiPreAssign[iTaskID] = i;
			}
		}
		
	}
}