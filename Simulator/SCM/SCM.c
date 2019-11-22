#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "gnuplot_i/src/gnuplot_i.h"


#define MULTITRACE
/*
#define ULTRIXTRACE
#define WARM_CACHE
*/

#define SEQ_LENGTH(sequencePtr)	((sequencePtr)->endBlknum-(sequencePtr)->startBlknum+1)

/* Pentium unsigned int = 4bytes */
#define INFINITY	0xFFFFFFFF

/* detected reference pattern */
#define SEQ		0	/* sequential references */
#define LOOP		1	/* looping references */
#define OTHERS		2	/* other references */

#define REAL            0	/* real buffers */

#define FALSE           0
#define TRUE            1

#define	MAXBUF		50000	/* max (real+ghost) buffer cache size */
#define	BLKHASHBUCKET	23617	/* hash bucket size */
				/* We use a hashing function to find a buffer with 
				   a given block number */

#define NUMofLRUSEGs	5	/* Num. of LRU segments */
#define REALLRU		2+NUMofLRUSEGs
#define MAXLRU		REALLRU+NUMofLRUSEGs+1

/* the cache partition where blocks are kept in the buffer cache */
#define SEQMRU		0	/* cache partition for sequential references */
#define FREEMRU		1	/* free buffers */

				/* cache partition for other references */
#define OTHLRU0         2	/* LRU segment list */
#define OTHLRU1		3	/* We measure buffer hit ratios at the five cache sizes */
#define OTHLRU2		4
#define OTHLRU3		5
#define OTHLRU4		6	
/* REALLRU */

#define FREEGHOST       7	/* free ghost buffesr */
#define GHOSTLRU0       8	/* GHOST segment list */
#define GHOSTLRU1       9	/* GHOSTLRU[i] corresponds to OTHLRU[i] */
#define GHOSTLRU2       10
#define GHOSTLRU3       11
#define GHOSTLRU4       12	
/* MAXLRU */
				/* cache partition for looping references */
#define LOOPQUEUE       13


#define MAXSEQLOOPs	2000	/* MAX detection table size */
#define	SEQLOOPHASHBUCKET	51 	/* hash bucket size */
				/* We use a hashing function to find a sequence with 
				   a given vnode number */

#define MINIMUMLRUSIZE	10 	/* We keep the minimum number of buffers for other references 
				   to predict the expected number of buffer hits per unit time */

struct buffer {			/* real buffer cache + ghost buffer cache */
	char	where;
	unsigned int	blkno;
	unsigned int	lastRefTime;
	unsigned int 	lastIRG;
	unsigned int	loopQindex;
	struct buffer *hashnext, *hashprev;
	struct buffer *lrunext, *lruprev;
} buf[MAXBUF];

struct buffer	lrulist[MAXLRU];	/* cache partition for sequential and other references 
					 	+ ghost buffers */

struct buffer	*loopQ[MAXBUF+1];	/* cache partition for looping references */

struct buffer	blkhash[BLKHASHBUCKET]; /* hash table */

unsigned int	cachesize = 100;	/* The total buffer cache size in the system */
unsigned int	allocBufInCache = 0;	/* The number of allocated real buffers */
unsigned int	allocBufInGhost = 0;	/* The number of allocated ghost buffers */

unsigned int 	ALLsegInitSize[MAXLRU];	/* The maximum size of each segment */
unsigned int 	ALLsegUsedSize[MAXLRU]; /* The current size of each segment */

unsigned int 	rLRUsegInitSizeSum = 0;
unsigned int	rLRUsegLoopQUsedSum = 0;
unsigned int	loopQused = 0;

unsigned int	agedVtime = 0;
unsigned int	curVtime = 0;
unsigned int	hit = 0;
unsigned int	miss = 0;
unsigned int	loopQhit=0;
unsigned int	rLRUsegHits[REALLRU];
unsigned int	rLRUsegHitRatios[NUMofLRUSEGs];


double	LRUsegRatio[NUMofLRUSEGs];

struct seQLoopStruct {			 /* detection table */
    unsigned int vnode;
    unsigned int startBlknum;
    unsigned int endBlknum;
    unsigned int curBlknum;
    unsigned int period;
    unsigned int unitFirstRefTime;
    char detectionState;
    struct seQLoopStruct *hashnext, *hashprev;
} seQLoopDetectUnit[MAXSEQLOOPs];

struct seQLoopStruct seQLoopHash[SEQLOOPHASHBUCKET];

/* used to determine that a consecutive block reference is indeed sequential references */
unsigned int thresholdSeQSize = 10; 
double curMarginalGainOth = 0.0;
double expo_alpha = 0.5;	/* exponential average */

unsigned int checkAvgMarginalGain();
unsigned int putInSeqLoopDetect();

struct seQLoopStruct	*Lfindblk();
struct buffer	*findblk();
struct buffer	*allocbuf();
struct buffer   *lruout();
struct buffer   *mruout();
struct buffer   *loopQout();

FILE *seq_f;
FILE *loop_f;
FILE *other_f;
unsigned int lineNum=0;

#define PLOT_SIZE 1000
gnuplot_ctrl *h_main;
gnuplot_ctrl *h_other;
unsigned int first=0;
unsigned int term=1;

main(argc, argv)
int	argc;
char	**argv;
{
	int	ret;
	unsigned int	i,j,diffSize;
	unsigned int blkno,vnode,pblkno,pid,lpblkno;
	double tmpSize;

	char op ;
	FILE *fp;

#ifdef WARM_CACHE
	int 	warmFlag=0;
#endif

	ALLsegInitSize[0] = 1;	
	ALLsegInitSize[1] = 0;


	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 's')
				cachesize = atoi(argv[i+1]);
			if (argv[i][1] == 'r') 
				ALLsegInitSize[0] = atoi(argv[i+1]); 
			if (argv[i][1] == 'm')
				ALLsegInitSize[1] = atoi(argv[i+1]); 
			if (argv[i][1] == 't')
			    thresholdSeQSize = atoi(argv[i+1]); 
			if (argv[i][1] == 'a')
			    expo_alpha = atof(argv[i+1]); 
		}
	}

	/* We measure buffer hit ratios at the five cache sizes (10% 20% 40% 60% 100%) */
	LRUsegRatio[0] = 0.1;	
	LRUsegRatio[1] = 0.1;
	LRUsegRatio[2] = 0.2;
	LRUsegRatio[3] = 0.2;
	LRUsegRatio[4] = 0.4;
	
	diffSize = 0;
	if( (cachesize-ALLsegInitSize[0]-ALLsegInitSize[1])*LRUsegRatio[0]+0.5 > 50) {
	    for(i=2;i<REALLRU-1;i++) {
	       	tmpSize 
		    = (double)(cachesize-ALLsegInitSize[0]-ALLsegInitSize[1])*LRUsegRatio[i-2]+0.5;
		ALLsegInitSize[i] = (int)tmpSize;
		diffSize += ALLsegInitSize[i];
	    }
	    ALLsegInitSize[i] = cachesize -ALLsegInitSize[0] - ALLsegInitSize[1] - diffSize;
	}
	else {
	    /* for much smaller segment size */
	    diffSize += ALLsegInitSize[0]+ALLsegInitSize[1];
	    ALLsegInitSize[2] = 30;
	    ALLsegInitSize[3] = 60;
	    diffSize += ALLsegInitSize[2]+ALLsegInitSize[3];
	    for(i=4;i<REALLRU-1;i++) {
		ALLsegInitSize[i] = ALLsegInitSize[i-1]*2;
		if(diffSize+ALLsegInitSize[i] > cachesize) 
		    break;
		diffSize += ALLsegInitSize[i];
	    }
	    if(i == REALLRU-1) {
		ALLsegInitSize[i] = cachesize - diffSize;
	    }
	    else {
		ALLsegInitSize[i-1] += cachesize - diffSize;
		for(;i<REALLRU;i++) 
		    ALLsegInitSize[i] = 0;
	    }
	}

	ALLsegInitSize[FREEGHOST] = cachesize;
	
	rLRUsegInitSizeSum = 0;
	for(i=OTHLRU0;i<REALLRU;i++) 
	    rLRUsegInitSizeSum += ALLsegInitSize[i];
	
	if(rLRUsegInitSizeSum != (cachesize - ALLsegInitSize[0] - ALLsegInitSize[1]))
		printf("Critical Error !: the sum of LRU segments (%d) is different with (%d)\n",rLRUsegInitSizeSum,(cachesize - ALLsegInitSize[0]));

	if(cachesize+ALLsegInitSize[FREEGHOST] >= MAXBUF) {
	    printf("Maximum buffer size %d is exceeded \n",MAXBUF);
	    exit(1);
	}
	printf("Total cache size = %d(",cachesize);
	for(i=0;i<REALLRU;i++) 
	    printf("%d+",ALLsegInitSize[i]);
	printf("=%d)\n",rLRUsegInitSizeSum);
	printf("Total ghost size = %d\n", ALLsegInitSize[FREEGHOST]);
	
	/* parameters */
	printf("thresholdSeQSize=%d\n",thresholdSeQSize);

	init();
	plot_init();

	if((fp = fopen("test.txt", "r"))==NULL) {
		printf("Failed to open file.");
		exit(1);
	}

	while (1) {
#ifdef ULTRIXTRACE
		ret = scanf("%u %u %u\n", &blkno,&vnode,&pblkno);
#endif 
#ifdef MULTITRACE 
		ret = fscanf(fp,"%u %u %u %c\n", &blkno,&vnode,&pblkno,&op);
		printf("input data : %u %u %u %c\n", blkno,vnode,pblkno,op);
		//ret = scanf("%u %u %u %u %c\n", &blkno,&vnode,&pblkno,&pid,&op);
		//ret = scanf("%u %u %u %u\n",&blkno,&vnode,&pblkno,&pid);
		//printf("%u %u %u %u %c\n", blkno,vnode,pblkno,pid,op);
#endif

		//if(ret == EOF)
		//	continue;

		lineNum++;
		curVtime++;
		agedVtime++;

#ifdef WARM_CACHE
		if(warmFlag == 0 && curVtime == 60001) {
		    warmFlag = 1;
		    agedVtime = 0;
		    curVtime = 1;
		    hit = 0;
		    miss = 0;
		    loopQhit=0;
		    for(i=0;i<MAXSEQLOOPs;i++)  {
			seQLoopDetectUnit[i].vnode =0;
			seQLoopDetectUnit[i].startBlknum =0;
			seQLoopDetectUnit[i].endBlknum =-1;
			seQLoopDetectUnit[i].curBlknum =0;
			seQLoopDetectUnit[i].period = -1;
			seQLoopDetectUnit[i].unitFirstRefTime =0;
			seQLoopDetectUnit[i].detectionState =OTHERS;
			seQLoopDetectUnit[i].hashprev = seQLoopDetectUnit[i].hashnext = NULL;
		    }
		    for(i=0;i<SEQLOOPHASHBUCKET;i++)  {
			seQLoopHash[i].vnode =0;
			seQLoopHash[i].startBlknum =0;
			seQLoopHash[i].endBlknum =-1;
			seQLoopHash[i].curBlknum =0;
			seQLoopHash[i].period = -1;
			seQLoopHash[i].detectionState =OTHERS;
			seQLoopHash[i].unitFirstRefTime =0;
			seQLoopHash[i].hashprev=seQLoopHash[i].hashnext=&(seQLoopHash[i]);
		    }
		    for(i=0;i<REALLRU;i++) 
			rLRUsegHits[i] = 0;
		    for(i=0;i<NUMofLRUSEGs;i++) 
			rLRUsegHitRatios[i] = 0;
		}
#endif
		
		//if(op == 'W') {
			pgref(blkno,vnode,pblkno);	/* page(disk block) reference */
		//}

		/* we periodically reset seQLoopDetectUnit value */
		if(curVtime%50000 == 0) {
		    for(i=0;i<NUMofLRUSEGs;i++) 
			rLRUsegHitRatios[i] = rLRUsegHitRatios[i] * 0.5;
		    agedVtime = agedVtime * 0.5;
		}

		if(curVtime%1000 == 0) { 
		    for(i=1;i<=loopQused;i++) {
			if((loopQ[i])->lastRefTime+(loopQ[i])->lastIRG*3 < curVtime) {
			    /* Loop periods of blocks are aged*/
			    (loopQ[i])->lastIRG = curVtime-(loopQ[i])->lastRefTime;
			    reorder(0,loopQ[i]);
			}
		    }
		}
		/*if(curVtime%100000 == 0) {
			term++;
		}*/
		if(curVtime%10000 == 0) {
			replot();
			printf("Hit ratio = %f\n(", (float) hit / (float) curVtime);
		}
	}
	printstat();

	printf("Final cache size ");
	for(i=0;i<REALLRU;i++) 
	    printf("%d ",ALLsegInitSize[i]);
	printf("=%d",rLRUsegInitSizeSum);
	printf("\n");

	printf("Final cache used ");
	for(i=0;i<REALLRU;i++) 
	    printf("%d ",ALLsegUsedSize[i]);
	printf("=%d",rLRUsegLoopQUsedSum-loopQused);
	printf("\n");

	printf("Final priority queue used %d\n",loopQused);
	printf("Final free ghost %d (",ALLsegUsedSize[FREEGHOST]);
	for(i=0;i<NUMofLRUSEGs;i++) 
	    printf("%d,",ALLsegUsedSize[GHOSTLRU0+i]);
	printf(")\n");
	printf("\n");

	f_close();
}

plot_init()
{
	h_main = gnuplot_init();
	gnuplot_cmd(h_main, "set terminal x11");
	gnuplot_cmd(h_main, "set key font \",14\"");
	gnuplot_cmd(h_main, "set xrange[0:16000000]");
	gnuplot_cmd(h_main, "set yrange[0:40000000]");
	gnuplot_set_xlabel(h_main,"Virtual time");
	gnuplot_set_ylabel(h_main,"Logical block address");

	h_other = gnuplot_init();
	gnuplot_cmd(h_other, "set terminal x11");
	gnuplot_cmd(h_other, "set key font \",14\"");
	gnuplot_cmd(h_other, "set xrange[0:16000000]");
	gnuplot_cmd(h_other, "set yrange[0:1500000]");
	gnuplot_set_xlabel(h_other,"Virtual time");
	gnuplot_set_ylabel(h_other,"Logical block address");
}

replot()
{
	if(first == 0) {
		first = 1;
		gnuplot_cmd(h_main, "plot \"other.txt\" lt rgb \"green\" title \"OTHERS\",\"seq.txt\" lt rgb \"red\" title \"SEQ\",\"loop.txt\" lt rgb \"blue\" title \"LOOP\"");
		gnuplot_cmd(h_other, "plot \"other.txt\" lt rgb \"green\" title \"OTHERS\",\"seq.txt\" lt rgb \"red\" title \"SEQ\",\"loop.txt\" lt rgb \"blue\" title \"LOOP\"");
		//gnuplot_cmd(h_main, "plot \"seq.txt\" lt rgb \"red\" title \"SEQ\",\"loop.txt\" lt rgb \"blue\" title \"LOOP\"");
		//gnuplot_cmd(h_other, "plot \"other.txt\" lt rgb \"green\" title \"OTHERS\"");
	}else {
		gnuplot_cmd(h_main, "replot");
		gnuplot_cmd(h_other, "replot");
	}
	//gnuplot_cmd(h, "pause 0.5");
	usleep(500000);
}

init()
{
	unsigned int	i;

	for (i = 0; i < MAXBUF+1; i++) loopQ[i] = NULL;

	for (i = 0; i < MAXBUF; i++) {
		buf[i].blkno = 0;
		buf[i].where = 0;
		buf[i].lastRefTime = 0;
		buf[i].lastIRG = 0;
		buf[i].loopQindex = 0;
		buf[i].hashnext = buf[i].hashprev = NULL;
		buf[i].lrunext = buf[i].lruprev = NULL;
	}
	for (i = 0; i < BLKHASHBUCKET; i++) {
		blkhash[i].blkno = 0;
		blkhash[i].where = 0;
		blkhash[i].lastRefTime = 0;
		blkhash[i].lastIRG = 0;
		blkhash[i].loopQindex = 0;
		blkhash[i].lrunext = blkhash[i].lruprev = NULL;
		blkhash[i].hashnext = blkhash[i].hashprev = &(blkhash[i]);
	}

	for(i=0;i<MAXLRU;i++) {
	    lrulist[i].lrunext = lrulist[i].lruprev = &(lrulist[i]);
	    ALLsegUsedSize[i] = 0;
	}

	for(i=0;i<NUMofLRUSEGs;i++) {
	    rLRUsegHitRatios[i] = 0;
	}

	for(i=0;i<REALLRU;i++) 
	    rLRUsegHits[i] = 0;
	
	for(i=0;i<MAXSEQLOOPs;i++)  {
	    seQLoopDetectUnit[i].vnode =0;
	    seQLoopDetectUnit[i].startBlknum =0;
	    seQLoopDetectUnit[i].endBlknum =-1;
	    seQLoopDetectUnit[i].curBlknum =0;
	    seQLoopDetectUnit[i].period = -1;
	    seQLoopDetectUnit[i].unitFirstRefTime =0;
	    seQLoopDetectUnit[i].detectionState =OTHERS;
	    seQLoopDetectUnit[i].hashprev = seQLoopDetectUnit[i].hashnext = NULL;
	}
	for(i=0;i<SEQLOOPHASHBUCKET;i++)  {
	    seQLoopHash[i].vnode =0;
	    seQLoopHash[i].startBlknum =0;
	    seQLoopHash[i].endBlknum =-1;
	    seQLoopHash[i].curBlknum =0;
	    seQLoopHash[i].period = -1;
	    seQLoopHash[i].detectionState =OTHERS;
	    seQLoopHash[i].unitFirstRefTime =0;
	    seQLoopHash[i].hashprev=seQLoopHash[i].hashnext=&(seQLoopHash[i]);
	}

	if((seq_f = fopen("seq.txt", "w"))==NULL) {
		printf("Failed to open file.");
		exit(1);
	}

	if((loop_f = fopen("loop.txt", "w"))==NULL) {
		printf("Failed to open file.");
		exit(1);
	}

	if((other_f = fopen("other.txt", "w"))==NULL) {
		printf("Failed to open file.");
		exit(1);
	}

}

detect_save(detectResult, blkno, vnode, pblkno, period)
unsigned int detectResult,blkno, vnode, pblkno, period;
{
//lineNum, blk
	switch(detectResult){
	case SEQ:
		fprintf(seq_f,"%u %u %u %u\n", lineNum, blkno, vnode, pblkno);
		printf("%s %u %u %u %u\n", "SEQ", blkno, vnode, pblkno, period);
		break;
	case LOOP:
		fprintf(loop_f,"%u %u %u %u\n", lineNum, blkno, vnode, pblkno);
		printf("%s %u %u %u %u\n", "LOOP", blkno, vnode, pblkno, period);
		break;
	case OTHERS:
		fprintf(other_f,"%u %u %u %u\n", lineNum, blkno, vnode, pblkno);
		printf("%s %u %u %u %u\n", "OTHERS", blkno, vnode, pblkno, period);
		break;
	}
}

f_close() {
	fclose(seq_f);
	fclose(loop_f);
	fclose(other_f);

	gnuplot_close(h_main);
	gnuplot_close(h_other);
}

pgref(blkno,vnode,pblkno)	/* called when page(disk block) is referenced */
unsigned int	blkno,vnode,pblkno;
{
    unsigned int 	i,j,where,delta,index,curLoopPeriod=0,detectResult;
    struct buffer	*bufp;
    struct buffer   	*tmpbufp,*ghostbufp;

	tmpbufp=NULL;
	bufp = findblk(blkno);

	if(bufp) {
	    delta = curVtime - bufp->lastRefTime;
	    curLoopPeriod= delta;
	}
	else curLoopPeriod = 0;
	    
	detectResult = putInSeqLoopDetect(vnode,pblkno,&curLoopPeriod);
	detect_save(detectResult, blkno, vnode, pblkno, curLoopPeriod);
	
	if (bufp != NULL 
		&& bufp->where >= 0 && bufp->where < REALLRU) {
		/* blk exists in real LRU buffers */

		    hit++;
		    rLRUsegHits[bufp->where]++;

		    if(bufp->where >= OTHLRU0 && bufp->where < OTHLRU0+NUMofLRUSEGs)
			rLRUsegHitRatios[bufp->where-OTHLRU0]++;


    		    bufp->lastRefTime = curVtime;
    		    bufp->lastIRG = curLoopPeriod;
   		    rmfromlrulist(bufp);
		    ALLsegUsedSize[bufp->where]--;
		    if(bufp->where >= OTHLRU0 && bufp->where < OTHLRU0+NUMofLRUSEGs)
			rLRUsegLoopQUsedSum--;
		    
		    if(detectResult == SEQ) {
			if(ALLsegUsedSize[SEQMRU] >= ALLsegInitSize[SEQMRU]) {
			    tmpbufp = lruout(SEQMRU);
			    lruin(FREEMRU,tmpbufp);
			}
			lruin(SEQMRU,bufp);
		    }
		    else if(detectResult == OTHERS) {
			i = OTHLRU0;
			tmpbufp = bufp;
			while(1) {
			    lruin(i,bufp);
			    if(i < OTHLRU0+NUMofLRUSEGs-1 
				    && ALLsegUsedSize[i] > ALLsegInitSize[i]
				    && ALLsegInitSize[i] > 0){
				bufp = lruout(i);
				i++;
			    }
			    else break;
			}
			bufp = tmpbufp;
		    }
		    else { /* LOOP */
			if(bufp->where == SEQMRU || bufp->where == FREEMRU)
			    loopQin(bufp);
			else {
			    i = bufp->where+1;
			    while(i<OTHLRU0+NUMofLRUSEGs) {
				if(ALLsegUsedSize[i] > 0) {
				    tmpbufp = mruout(i);
				    insertLRU(i-1,tmpbufp);
				    i++;
				}
				else break;
			    }
			    i--;
			    j = i+NUMofLRUSEGs+2;
			    while(j<GHOSTLRU0+NUMofLRUSEGs && ALLsegInitSize[i] > 0) {
				if(ALLsegUsedSize[j] > 0) {
				    tmpbufp = mruout(j);
				    insertLRU(j-1,tmpbufp);
				    j++;
				    i++;
				}
				else break;
			    }
			    loopQin(bufp);
			}
		    }
	}
	else if(bufp != NULL && bufp->where == LOOPQUEUE) {
	/* blk exists in real LoopQ queues */

		    hit++;
		    loopQhit++;

		    /* to remove the block from the LoopQ queue*/
		    bufp->lastIRG = INFINITY;
    		    bufp->lastRefTime = curVtime;
		    reorder(0,bufp);
		    /* 0: upheap, 1:downheap */
		    bufp = loopQout();
		    if(bufp->blkno != blkno) {
		   	printf("Critical Error!: block numbers are different with each other \n");
		    }
		    bufp->lastIRG = curLoopPeriod; 
    		    bufp->lastRefTime = curVtime;

		    if(detectResult == LOOP)
			loopQin(bufp);
		    else {
			i = OTHLRU0;
			tmpbufp = bufp;
			while(1) {
			    lruin(i,bufp);
			    if(i < OTHLRU0+NUMofLRUSEGs-1 
				    && ALLsegUsedSize[i] > ALLsegInitSize[i]
				    && ALLsegInitSize[i] > 0){
				bufp = lruout(i);
				i++;
			    }
			    else break;
			}
			
			/* adjust ghost buffers */
			i = bufp->where;
		    	j = i+NUMofLRUSEGs+1;
				
			if(j < GHOSTLRU0+NUMofLRUSEGs-1 && ALLsegInitSize[i] > 0 
				    && ALLsegUsedSize[j]+ALLsegUsedSize[i] > ALLsegInitSize[i]) {
			    bufp = lruout(j,bufp);
			    i++;
			    j++;
			    while(1) {
				lruin(j,bufp);
				if(j < GHOSTLRU0+NUMofLRUSEGs-1 && ALLsegInitSize[i] > 0 
				    && ALLsegUsedSize[j]+ALLsegUsedSize[i] > ALLsegInitSize[i]) {
			    	    bufp = lruout(j);
			    	    i++;
			    	    j++;
			       	}
				else break;
			    }
			}
			bufp = tmpbufp;
		    }
	}
	else {

	    if(bufp != NULL) {
	       if(bufp->where >= FREEGHOST && bufp->where <= FREEGHOST+NUMofLRUSEGs) {
		   ghostbufp = bufp;
		   if(bufp->where >= GHOSTLRU0 && bufp->where < GHOSTLRU0+NUMofLRUSEGs)
		       rLRUsegHitRatios[bufp->where-GHOSTLRU0]++;
	       }
	       else 
		   printf("Critical! unknown where %d\n",bufp->where);
	    }
	    else ghostbufp = NULL;
	
    	    miss++;

		if (allocBufInCache < cachesize) {	
		/* cache is not full. warming up */
			bufp = allocbuf(REAL);
			bufp->blkno = blkno;
			bufp->lastRefTime = curVtime;
			bufp->lastIRG = curLoopPeriod;

			hashin(bufp);
			if(detectResult == OTHERS) {
			    i = OTHLRU0;
			    tmpbufp=bufp;
			    while(1) {
				lruin(i,bufp);
				if(i < OTHLRU0+NUMofLRUSEGs-1 
					&& ALLsegUsedSize[i] > ALLsegInitSize[i]
					&& ALLsegInitSize[i] > 0) {
				    bufp = lruout(i);
				    i++;
				}
				else break;
			    }
			    bufp = tmpbufp;
			}
			else {
			    if(detectResult == LOOP)
				printf("%d: Unknown error!! \n",curVtime);

			    if(ALLsegUsedSize[SEQMRU] >= ALLsegInitSize[SEQMRU]) {
			       	tmpbufp = lruout(SEQMRU);
				lruin(FREEMRU,tmpbufp);
			    }
			    lruin(SEQMRU,bufp);
			}
		}
		else {

		    /* find a victim ghost block */
		    if(ghostbufp == NULL) {
			if(allocBufInGhost < ALLsegInitSize[FREEGHOST]) 
			    tmpbufp = allocbuf(FREEGHOST);
			else {
			    if(ALLsegUsedSize[FREEGHOST] > 0)
			       	tmpbufp = lruout(FREEGHOST);
			    else {
				j = GHOSTLRU0+NUMofLRUSEGs-1;
				while(1) {
				    if(ALLsegUsedSize[j] > 0) {
					tmpbufp = lruout(j);
					break;
				    }
				    else j--;
				}
			    }

			    hashout(tmpbufp);
			}
		    }
		    else {
			rmfromlrulist(ghostbufp);
			if(ghostbufp->where == FREEGHOST)
			    ALLsegUsedSize[FREEGHOST]--;

			if(ghostbufp->where >= GHOSTLRU0 
				&& ghostbufp->where < GHOSTLRU0+NUMofLRUSEGs) {
			    struct buffer *tmpbufp2;

			    ALLsegUsedSize[ghostbufp->where]--;

			    j = ghostbufp->where+1;
			    while(j<GHOSTLRU0+NUMofLRUSEGs) {
				if(ALLsegUsedSize[j] > 0) {
				    tmpbufp2 = mruout(j);
				    insertLRU(j-1,tmpbufp2);
				    j++;
				}
				else break;
			    }
			}

			tmpbufp = ghostbufp;
		    }

		    tmpbufp->lastIRG = curLoopPeriod;
		    tmpbufp->lastRefTime = curVtime;
		    tmpbufp->blkno = blkno;	/* replace buf with new block */
			
		    /* find a victim real block */

		    if(ALLsegUsedSize[SEQMRU]+ALLsegUsedSize[FREEMRU] > 
			    ALLsegInitSize[SEQMRU]+ALLsegInitSize[FREEMRU]) {
			bufp = mruout(FREEMRU);		
		    }
		    else { 
			if(detectResult == SEQ) {
			    if(ALLsegUsedSize[SEQMRU] >= ALLsegInitSize[SEQMRU]) 
				bufp = lruout(SEQMRU);		
			    else if(rLRUsegLoopQUsedSum-loopQused <= MINIMUMLRUSIZE ||
				    (curMarginalGainOth > 0.0 && loopQused > 0 
				    && 1.0/loopQ[1]->lastIRG < curMarginalGainOth))
				bufp = loopQout();
			    else {
				i = OTHLRU0+NUMofLRUSEGs-1;
				while(1) {
				    if(ALLsegUsedSize[i] > 0) {
					bufp = lruout(i);
					break;
				    }
				    else i--;
				}
			    }
			}
			else { /* LOOP or OTHERS */
			    where = deallocator(detectResult,curLoopPeriod); 
			    if(where == OTHLRU0) {
				i = OTHLRU0+NUMofLRUSEGs-1;
				while(1) {
				    if(ALLsegUsedSize[i] > 0) {
					bufp = lruout(i);
					break;
				    }
				    else i--;
				}
			    }
			    else if(where == LOOPQUEUE) {
				bufp = loopQout();
			    }
			    else {
				detectResult = SEQ;
				if(ALLsegUsedSize[SEQMRU] == 0) {
				    i = OTHLRU0+NUMofLRUSEGs-1;
				    while(1) {
					if(ALLsegUsedSize[i] > 0) {
					    bufp = lruout(i);
					    break;
					}
					else i--;
				    }
				}
				else bufp = lruout(SEQMRU);		
			    }
			}
		    }


		    if(bufp->where >= OTHLRU0 && bufp->where < OTHLRU0+NUMofLRUSEGs) {
			i = bufp->where;
			
		    	j = i+NUMofLRUSEGs+1;
	    		while(1) {
    			    lruin(j,bufp);
			    if(j < GHOSTLRU0+NUMofLRUSEGs-1 && ALLsegInitSize[i] > 0 
				    && ALLsegUsedSize[j]+ALLsegUsedSize[i] > ALLsegInitSize[i]) {
				bufp = lruout(j);
				i++;
				j++;
			    }
			    else break;
			}
		    }
		    else lruin(FREEGHOST,bufp);

		    if(ghostbufp == NULL) 
			hashin(tmpbufp);

		    if(detectResult == LOOP) {
			loopQin(tmpbufp);
		    }
		    else if(detectResult == SEQ) {
			if(ALLsegUsedSize[SEQMRU] >= ALLsegInitSize[SEQMRU]) {
	    		    bufp = lruout(SEQMRU);
    			    lruin(FREEMRU,bufp);
			}
			lruin(SEQMRU,tmpbufp);
		    }
		    else {
			i = OTHLRU0;
	    		bufp=tmpbufp;
		    	while(1) {
	    		    lruin(i,bufp);
    			    if(i < OTHLRU0+NUMofLRUSEGs-1 
				    && ALLsegUsedSize[i] > ALLsegInitSize[i] 
				    && ALLsegInitSize[i] > 0){
				bufp = lruout(i);
				i++;
			    }
			    else break;
			}

			/* adjust ghost buffers */
			i = bufp->where;
		    	j = i+NUMofLRUSEGs+1;
				
			if(j < GHOSTLRU0+NUMofLRUSEGs-1 && ALLsegInitSize[i] > 0 
				    && ALLsegUsedSize[j]+ALLsegUsedSize[i] > ALLsegInitSize[i]) {
			    bufp = lruout(j,bufp);
			    i++;
			    j++;
			    while(1) {
				lruin(j,bufp);
				if(j < GHOSTLRU0+NUMofLRUSEGs-1 && ALLsegInitSize[i] > 0 
				    && ALLsegUsedSize[j]+ALLsegUsedSize[i] > ALLsegInitSize[i]) {
			    	    bufp = lruout(j);
			    	    i++;
			    	    j++;
			       	}
				else break;
			    }
			}
		    }
			
		    bufp = tmpbufp;
		}
	}
	
}

printstat()
{
    unsigned int i;
	printf("\ntotal access = %ld, hit = %d(",curVtime, hit);
	for(i=0;i<REALLRU;i++)
	    printf("%d,",rLRUsegHits[i]);
	printf("%d",loopQhit);
	printf(") miss = %d\n",miss);
	printf("Hit ratio = %f\n(", (float) hit / (float) curVtime);
	for(i=0;i<REALLRU;i++)
	    printf("%f,",(float)rLRUsegHits[i]/(float)curVtime);
	printf("%f",(float)loopQhit/(float)curVtime);
	printf(")\n");

	/* hit ratio that does not include hits in loops */
	{
	    unsigned int Total=0;
	    printf("Hit ratio per region = ");
	    for(i=0;i<NUMofLRUSEGs;i++) {
	       	Total += rLRUsegHitRatios[i];
	    	printf("%f,",(double)Total/(double)agedVtime);
		rLRUsegHitRatios[i] = Total;
	    }
	    printf("\n");
	}

#ifdef ULTRIXTRACE
	printf("Miss ratio = %f\n", 1.0-(float) hit / (float) curVtime);
#endif
	
	printf("\n");

	if(loopQused > 0)
	    printf("Last IRG block's Marginal Gain: %f\n",1.0/(double)loopQ[1]->lastIRG);
	printf("Last LRU block's Marginal Gain: %f\n",curMarginalGainOth);
	
	printf("\n");
}

struct buffer *
allocbuf(type)
    int type;
{
    if(type == REAL){
	allocBufInCache++;
	return(&(buf[allocBufInCache + allocBufInGhost]));
    }
    else {
	allocBufInGhost++;
	return(&(buf[allocBufInCache + allocBufInGhost]));
    }
}

/* Functions which are related hash to find buffer containing
	disk block whose block number is "blkno" */

struct buffer *
findblk(blkno)
int	blkno;
{
	int	off;
	struct buffer *dp, *bp;

	off = blkno % BLKHASHBUCKET;
	dp = &(blkhash[off]);
	for (bp = dp->hashnext; bp != dp; bp = bp->hashnext) {
		if (bp->blkno == blkno) {
			return(bp);
		}
	}
	return(NULL);
}

hashin(bp)
struct buffer *bp;
{
	int	off;
	struct buffer *dp;

	off = bp->blkno % BLKHASHBUCKET;
	dp = &(blkhash[off]);
	bp->hashnext = dp->hashnext;
	bp->hashprev = dp;
	(dp->hashnext)->hashprev = bp;
	dp->hashnext = bp;
}

hashout(bp)
struct buffer *bp;
{
	(bp->hashprev)->hashnext = bp->hashnext;
	(bp->hashnext)->hashprev = bp->hashprev;
	bp->hashnext = bp->hashprev = NULL;
}

/* LRU List management functions */

/* Remove and return LRU(Least Recently Used block from LRU list */
struct buffer *
lruout(type)
    int type;
{
	struct buffer *bp;

	ALLsegUsedSize[type]--;

	if(type >= OTHLRU0 && type < OTHLRU0+NUMofLRUSEGs)
	    rLRUsegLoopQUsedSum--;
	bp = lrulist[type].lruprev;
	(bp->lruprev)->lrunext = bp->lrunext;
	(bp->lrunext)->lruprev = bp->lruprev;
	bp->lrunext = bp->lruprev = NULL;

	return(bp);
}


lruin(type,bp)			/* Insert a block to the MRU location 
				   in LRU list */
int type;
struct buffer *bp;
{
	struct buffer *dp;

	if(type >= OTHLRU0 && type < OTHLRU0+NUMofLRUSEGs)
	    rLRUsegLoopQUsedSum++;

	ALLsegUsedSize[type]++;

	dp = &lrulist[type];
	bp->where = type;
	bp->lrunext = dp->lrunext;
	bp->lruprev = dp;
	(dp->lrunext)->lruprev = bp;
	dp->lrunext = bp;
}

struct buffer *
mruout(type)
    int type;
{
	struct buffer *bp;

	ALLsegUsedSize[type]--;

	if(type >= OTHLRU0 && type < OTHLRU0+NUMofLRUSEGs)
	    rLRUsegLoopQUsedSum--;

	bp = lrulist[type].lrunext;
	(bp->lruprev)->lrunext = bp->lrunext;
	(bp->lrunext)->lruprev = bp->lruprev;
	bp->lrunext = bp->lruprev = NULL;

	return(bp);
}
insertLRU(type,bp)			/* Insert a block to the LRU location 
					   in LRU list */
int type;
struct buffer *bp;
{
	struct buffer *dp;

	ALLsegUsedSize[type]++;

	if(type >= OTHLRU0 && type < OTHLRU0+NUMofLRUSEGs)
	    rLRUsegLoopQUsedSum++;

	dp = &lrulist[type];
	bp->where = type;
	
	bp->lrunext = dp;
	bp->lruprev = dp->lruprev;
	(dp->lruprev)->lrunext = bp;
	dp->lruprev = bp;
}




/* Remove a block from LRU list */
rmfromlrulist(bp)
struct buffer *bp;
{
	(bp->lruprev)->lrunext = bp->lrunext;
	(bp->lrunext)->lruprev = bp->lruprev;
	bp->lrunext = bp->lruprev = NULL;
}

/* Priority Queue Management Functions.
   See "Algorithms" written by Robert Sedgewick, pp 132-135 */

/* Replace a buffer whose value is least with new buffer pointed by bp */
/* Note that loopQ[1] points a buffer containing block whose value is least */

/* Remove a block whose value is least from Priority Queue */
struct buffer *
loopQout()
{
	unsigned int    k1,k2;
	struct buffer *bp,*swap;

	rLRUsegLoopQUsedSum--;
	
	/* considering correlated references */
	if(loopQused > 1 && curVtime > loopQ[1]->lastRefTime 
		&& (curVtime-loopQ[1]->lastRefTime) <= ALLsegInitSize[SEQMRU]) {
	    if(loopQused == 2) {
		bp = loopQ[2];
		loopQ[2] = loopQ[1];
		(loopQ[2])->loopQindex = 2;
	    }
	    else {
		k1 = loopQ[2]->lastIRG;
		k2 = loopQ[3]->lastIRG;
		/* k1 is more valuable */
		if ((k1 < k2) ||
		    ((k1==k2)&&(loopQ[2]->lastRefTime<loopQ[3]->lastRefTime)) ) {
		    bp = loopQ[3];
		    loopQ[3] = loopQ[1];
		    (loopQ[3])->loopQindex = 3;
		}
		else {
		    bp = loopQ[2];
		    loopQ[2] = loopQ[1];
		    (loopQ[2])->loopQindex = 2;
		}
	    }
	}
	else bp = loopQ[1];

	bp->loopQindex = 0;

	if (loopQused > 1) {
		loopQ[1] = loopQ[loopQused];
		loopQ[loopQused--] = NULL;

		(loopQ[1])->loopQindex = 1;
		downheap(1);
	}
	else {
		loopQ[loopQused--] = NULL;
	}

	return(bp);
}

downheap(index)
int	index;
{
	int		j;
	struct buffer	*tmpbp;
	unsigned int    k1,k2;

	while (index <= (loopQused/2)) {
	   j = index+index;
	   if (j < loopQused) {
	       k1 = loopQ[j]->lastIRG;
	       k2 = loopQ[j+1]->lastIRG;
	       /* k1 is more valuable */
	       if ((k1 < k2) ||
		 ((k1==k2)&&(loopQ[j]->lastRefTime<loopQ[j+1]->lastRefTime)) )
		   j++;

	   }
	   k1 = loopQ[index]->lastIRG;
	   k2 = loopQ[j]->lastIRG;
	   /* k1 is more valuable */
	   if ((k1 < k2) ||
	       ((k1==k2)&&(loopQ[index]->lastRefTime<loopQ[j]->lastRefTime)) ) {
		(loopQ[index])->loopQindex = j;
		(loopQ[j])->loopQindex = index;
		tmpbp = loopQ[index];
		loopQ[index] = loopQ[j];
		loopQ[j] = tmpbp;
	   }
	   else break;
	   index = j;
	}
}

/* Insert a block to Priority Queue */
loopQin(bp)
struct buffer *bp;
{

	rLRUsegLoopQUsedSum++;
	loopQused++;
	loopQ[loopQused] = bp;
	bp->loopQindex = loopQused;
	bp->where = LOOPQUEUE;
	upheap(loopQused);
}

upheap(index)
int	index;
{
	struct buffer	tmpbuf, *tmpbp;
	unsigned int    k1,k2;

	loopQ[0] = &(tmpbuf);
	tmpbuf.lastIRG = 0;
	tmpbuf.lastRefTime = 0L;

	k1 = loopQ[index/2]->lastIRG;
	k2 = loopQ[index]->lastIRG;
	/* k1 is more valuable */
	while ( (index > 1) && ( (k1 < k2) || 
     (k1 == k2 && loopQ[index/2]->lastRefTime<loopQ[index]->lastRefTime) ) ) {
		(loopQ[index])->loopQindex = index/2;
		(loopQ[index/2])->loopQindex = index;

		tmpbp = loopQ[index/2];
		loopQ[index/2] = loopQ[index];
		loopQ[index] = tmpbp;
		index = index/2;
		k1 = loopQ[index/2]->lastIRG;
		k2 = loopQ[index]->lastIRG;
	}
}

/* Reorder a block according to new value in the Priority Queue */
reorder(dir, bp)
int	dir;			/* 0 : upheap, 1 : downheap */
struct buffer	*bp;
{
	if (dir == 0) {
		upheap(bp->loopQindex);
	}
	else {
		downheap(bp->loopQindex);
	}
}


unsigned int
putInSeqLoopDetect(vnode,pblkno,retIRG)
    unsigned int vnode, pblkno, *retIRG;
{
    struct seQLoopStruct *sequence,*tmpseq;
    unsigned int off,i,minsize=curVtime+1,minindex=0;

	
    off = 0;
    sequence = Lfindblk(vnode,&off);

    if(sequence != NULL && sequence->vnode == vnode) {
	if(sequence->detectionState == OTHERS) {
	    if(sequence->endBlknum-1 == pblkno
		    || sequence->endBlknum == pblkno 
		    || sequence->endBlknum+1 == pblkno) {
		/* increase sequential referenced block list */

		if(sequence->endBlknum+1 == pblkno) {
		    sequence->endBlknum = pblkno;
		    sequence->curBlknum = pblkno;
		}
		if(SEQ_LENGTH(sequence) < thresholdSeQSize) 
		    return(OTHERS);
		else if(sequence->period == -1){
		    sequence->detectionState = SEQ;
		    return(SEQ);
		}
		else {
		    printf("What is that?\n");
		    return(OTHERS);
		}
	    }
	    else {
		Lhashout(sequence);
		sequence->detectionState = OTHERS;
		sequence->vnode = 0;
		sequence->period = -1;
		sequence->startBlknum = 0;
		sequence->endBlknum = -1;
		sequence->curBlknum = 0;
		sequence->unitFirstRefTime = 0;
	    }
	}
	else if(sequence->detectionState == SEQ) {
	    if(sequence->endBlknum-1 == pblkno
		    || sequence->endBlknum == pblkno 
		    || sequence->endBlknum+1 == pblkno) {
		/* increase sequential referenced block list */

		if(sequence->endBlknum+1 == pblkno) {
		    sequence->endBlknum = pblkno;
		    sequence->curBlknum = pblkno;
		}
		return(SEQ);
	    }
	    else if(sequence->curBlknum == sequence->endBlknum && 
		    sequence->startBlknum == pblkno) {
		
	    	sequence->curBlknum = pblkno;
		
	    	if(sequence->period == -1 || sequence->period == 0) {
    		    if(*retIRG > 0) 
			sequence->period = *retIRG;
		    else 
			sequence->period 
			    = curVtime-sequence->unitFirstRefTime;
		}
		else {
		    if((curVtime-sequence->unitFirstRefTime) 
			    > SEQ_LENGTH(sequence)) {
			if(abs(sequence->period - 
				    (curVtime-sequence->unitFirstRefTime)) 
				> sequence->period * 0.3)
			    /* exponential average */
			    sequence->period = (int)((1.0-expo_alpha)*(double)sequence->period +expo_alpha*(double)(curVtime-sequence->unitFirstRefTime));
		    }
		}
		
	    	sequence->unitFirstRefTime = curVtime;
    		sequence->detectionState = LOOP;
		*retIRG = sequence->period;
		return(LOOP);
	    }
	    else {
		Lhashout(sequence);
		sequence->detectionState = OTHERS;
		sequence->vnode = 0;
		sequence->period = -1;
		sequence->startBlknum = 0;
		sequence->endBlknum = -1;
		sequence->curBlknum = 0;
		sequence->unitFirstRefTime = 0;
	    }
	}
	else { /* LOOP */
	    if(sequence->startBlknum == pblkno) { 
		sequence->curBlknum = pblkno;

	    	if(sequence->period == -1 || sequence->period == 0) {
    		    if(*retIRG > 0) 
			sequence->period = *retIRG;
		    else 
			sequence->period 
			    = curVtime-sequence->unitFirstRefTime;
		}
		else {
		    if((curVtime-sequence->unitFirstRefTime) 
			    > SEQ_LENGTH(sequence)) {
			if(abs(sequence->period - 
				    (curVtime-sequence->unitFirstRefTime)) 
				> sequence->period * 0.3)
			    /* exponential average */
			    sequence->period = (int)((1.0-expo_alpha)*(double)sequence->period +expo_alpha*(double)(curVtime-sequence->unitFirstRefTime));
		    }
		}

	    	sequence->unitFirstRefTime = curVtime;
    		sequence->detectionState = LOOP;
		*retIRG = sequence->period;
		return(LOOP);
	    }
	    else if(pblkno <= sequence->endBlknum && (
		    sequence->curBlknum-1 == pblkno ||
		    sequence->curBlknum == pblkno || 
		    sequence->curBlknum+1 == pblkno) ) {

	    	if(sequence->period == -1 || sequence->period == 0) {
    		    if(*retIRG > 0) 
			sequence->period = *retIRG;
		    else 
			sequence->period 
			    = curVtime-sequence->unitFirstRefTime;
		}
		
	    	*retIRG = sequence->period;
		
	    	if(pblkno == sequence->endBlknum+1) {
		    sequence->endBlknum = pblkno;
		}
		if(pblkno == sequence->curBlknum+1) 
		    sequence->curBlknum = pblkno;

	    	return(LOOP);
    	    }
	    else {				
		Lhashout(sequence);
		sequence->detectionState = OTHERS;
		sequence->vnode = 0;
		sequence->period = -1;
		sequence->startBlknum = 0;
		sequence->endBlknum = -1;
		sequence->curBlknum = 0;
		sequence->unitFirstRefTime = 0;
	    }
	}
    }
    
    if(pblkno == 0 || pblkno == 1) {
       	for(i=0;i<MAXSEQLOOPs;i++) {
    	    if(SEQ_LENGTH(&seQLoopDetectUnit[i]) < minsize) {
    		minsize = SEQ_LENGTH(&seQLoopDetectUnit[i]);
    		minindex = i;
    		if(minsize == 0)
    		    break;
    	    }
    	    else if(SEQ_LENGTH(&seQLoopDetectUnit[i]) == minsize) {
    		if(seQLoopDetectUnit[i].unitFirstRefTime 
    			< seQLoopDetectUnit[minsize].unitFirstRefTime) {
    		    minindex = i;
    		}
    	    }
       	}/* end for */


	if(minsize == curVtime)
    	    printf("Critical Error! In putInSeqLoopDetect() \n");

	if(seQLoopDetectUnit[minindex].vnode != 0)
    	    Lhashout(&seQLoopDetectUnit[minindex]);
       	seQLoopDetectUnit[minindex].vnode = vnode;
	seQLoopDetectUnit[minindex].startBlknum = 0;
	seQLoopDetectUnit[minindex].endBlknum = pblkno;
	seQLoopDetectUnit[minindex].curBlknum = pblkno;
	seQLoopDetectUnit[minindex].period = -1;
	seQLoopDetectUnit[minindex].detectionState = OTHERS;
	seQLoopDetectUnit[minindex].unitFirstRefTime = curVtime;
	Lhashin(&seQLoopDetectUnit[minindex]);
    }
    return(OTHERS);
}


unsigned int
checkAvgMarginalGain()
{
    unsigned int i,j,numIndex=0,total;
    unsigned int    tmprLRUsegHitRatios[NUMofLRUSEGs];
    double c[NUMofLRUSEGs],k[NUMofLRUSEGs],hit[NUMofLRUSEGs],size[NUMofLRUSEGs],checkSize;
    double avgC,avgK,oldMarginalGain,tmpMarginalGain;
    
#ifdef MULTITRACE
    if(ALLsegUsedSize[OTHLRU1] == 0) {
#else 
    if(ALLsegUsedSize[OTHLRU1]+ALLsegUsedSize[GHOSTLRU1] == 0) {
#endif
	return(FALSE);
    }
    
    i = OTHLRU0+NUMofLRUSEGs-1;
    j = GHOSTLRU0+NUMofLRUSEGs-1;
            
    while(1) {
	if(ALLsegUsedSize[i] > 0 && ALLsegInitSize[i] > 0) {
	    break;
	}
	else {
	    i--;
	    j--;
	}
    }
    
    numIndex = i-OTHLRU0+1;

    if(numIndex == 1)
	numIndex++;
    
    total = 0;
    for(i=0;i<numIndex;i++) {
	total += rLRUsegHitRatios[i];
	tmprLRUsegHitRatios[i] = total;
    }
    
    for(i=0;i<numIndex;i++) 
	hit[i] = (double)tmprLRUsegHitRatios[i]/(double)agedVtime;
    

    size[0] = ALLsegUsedSize[OTHLRU0];
    for(i=1;i<numIndex;i++)  
	size[i] = size[i-1]+ALLsegUsedSize[i+OTHLRU0]+ALLsegUsedSize[i+GHOSTLRU0]; 


    avgC = avgK = 0.0;

    for(i=1;i<numIndex;i++) {
	k[i] = log((1.0-hit[0])/(1.0-hit[i]))/log(size[i]/size[0]);
	avgK += k[i];
	c[i] = (1.0-hit[0])*pow(size[0],k[i]);
	avgC += c[i];
    }
    
    avgK = avgK/(double)(numIndex-1);
    avgC = avgC/(double)(numIndex-1);
    
    
    checkSize = (double)rLRUsegLoopQUsedSum-(double)loopQused;

    if(curMarginalGainOth > 0.0)
	curMarginalGainOth=(avgC/pow(checkSize,avgK)-avgC/pow(checkSize+1.0,avgK)+curMarginalGainOth)/2.0; 
    else curMarginalGainOth=avgC/pow(checkSize,avgK)-avgC/pow(checkSize+1.0,avgK); 


	printf("LRU block's Marginal Gain: %f\n",curMarginalGainOth);

    return(TRUE);
}

/* select which partition a victim block is replaced from */
deallocator(detectResult,curLoopPeriod) 
    unsigned int detectResult,curLoopPeriod;
{
	
    double tmpDelta;
    unsigned int baseIRG;
    
    if(loopQused >= rLRUsegInitSizeSum-MINIMUMLRUSIZE) {
	if(detectResult == LOOP) {
	    if(loopQ[1]->lastIRG > curLoopPeriod || ALLsegUsedSize[SEQMRU] == 0)
		return(LOOPQUEUE);
	    else 
		return(SEQMRU);
	}
	else 
	    return(LOOPQUEUE);
    }
				    
    if(checkAvgMarginalGain()) {

	if(detectResult == LOOP) {
    
	    if(loopQused > 0) {
		if(loopQ[1]->lastIRG > curLoopPeriod)
		    baseIRG = loopQ[1]->lastIRG;
		else 
		    baseIRG = curLoopPeriod;
	    }
	    else 
		baseIRG = curLoopPeriod;
	
	    tmpDelta =curMarginalGainOth-1.0/(double)baseIRG;
	
	    if(tmpDelta > 0.0) {
		/* The marginal gain value of the OTHER partition 
		   is larger than that of the LOOP partition */
		if(loopQused == 0) 
		    return(SEQMRU);
		else if(curLoopPeriod <  baseIRG || ALLsegUsedSize[SEQMRU] == 0)
		    return(LOOPQUEUE);
		else 
		    return(SEQMRU);
	    }
	    else if(tmpDelta < 0.0)
		/* The marginal gain value of the LOOP partition 
		   is larger than that of the OTHER partition */
		return(OTHLRU0);
	    else {
		/* When the marginal gain value of the OTHER partition 
		   is equal to that of the LOOP partition,
		   we select the LOOP partition or the SEQ partition first*/
		if(loopQused > 0 || ALLsegUsedSize[SEQMRU] == 0)
		    return(LOOPQUEUE);
		else 
		    return(SEQMRU);
	    }
	}
	else {
	    if(loopQused > 0) {
		tmpDelta =curMarginalGainOth-1.0/(double)loopQ[1]->lastIRG;
		if(tmpDelta > 0.0) 
		    return(LOOPQUEUE);
		else if(tmpDelta < 0.0)
		    return(OTHLRU0);
		else 
		    return(LOOPQUEUE);
	    }
	    else 
		return(OTHLRU0);
	} /* if(detectResult) */
    } /* if(checkMarginalGain .. */
    else {
	if(detectResult == LOOP && loopQused > 0) {
	    if(loopQ[1]->lastIRG > curLoopPeriod || ALLsegUsedSize[SEQMRU] == 0)
		return(LOOPQUEUE);
	    else 
		return(SEQMRU);
	}
	else if(detectResult == SEQ && ALLsegUsedSize[SEQMRU] > 0) 
	    return(SEQMRU);
	else 
	    return(OTHLRU0);
    }
}

/* Functions which are related hash to find buffer containing
	seQLoopStruct block whose number is "vnode" */

struct seQLoopStruct *
Lfindblk(vnode,retoff)
int	vnode,*retoff;
{
	int	off;
	struct seQLoopStruct *dp, *bp;

	off = vnode % SEQLOOPHASHBUCKET;
	*retoff = off;
	dp = &(seQLoopHash[off]);
	for (bp = dp->hashnext; bp != dp; bp = bp->hashnext) {
		if (bp->vnode == vnode) {
			return(bp);
		}
	}
	return(NULL);
}

Lhashin(bp)
struct seQLoopStruct *bp;
{
	int	off;
	struct seQLoopStruct *dp;

	off = bp->vnode % SEQLOOPHASHBUCKET;
	dp = &(seQLoopHash[off]);
	bp->hashnext = dp->hashnext;
	bp->hashprev = dp;
	(dp->hashnext)->hashprev = bp;
	dp->hashnext = bp;
}

Lhashout(bp)
struct seQLoopStruct *bp;
{
	(bp->hashprev)->hashnext = bp->hashnext;
	(bp->hashnext)->hashprev = bp->hashprev;
	bp->hashnext = bp->hashprev = NULL;
}

