////////////////////////////////////////////////////////////////////////////////
// Main File:        csim.c
// Semester:         CS 354 Fall 2016
//
// Author:           Qiannan Guo
// Email:            qguo43@wisc.edu
// CS Login:         qiannan
////////////////////////////////////////////////////////////////////////////////

/* Name:Qiannan Guo
 * CS login:qiannan
 * Section:002
 *
 *
 *
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *  2. Instruction loads (I) are ignored, since we are interested in evaluating
 *  trans.c in terms of its data cache performance.
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work. 
 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include<stdbool.h>

#include "cachelab.h"

// #define DEBUG_ON 
#define ADDRESS_LENGTH 64

/****************************************************************************/
/***** DO NOT MODIFY THESE VARIABLE NAMES ***********************************/

/* Globals set by command line args */
int verbosity = 0; /* print trace if set */
int s = 0; /* set index bits */
int b = 0; /* block offset bits */
int E = 0; /* associativity */
char* trace_file = NULL;

/* Derived from command line args */
int S; /* number of sets S = 2^s In C, you can use "pow" function*/
int B; /* block size (bytes) B = 2^b In C, you can use "pow" function*/

/* Counters used to record cache statistics */
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;
/*****************************************************************************/


/* Type: Memory address 
 * Use this type whenever dealing with addresses or address masks
  */
typedef unsigned long long int mem_addr_t;

/* Type: Cache line
 * TODO 
 * 
 * NOTE: 
 * You will need to add an extra field to this struct
 * depending on your implementation of the LRU policy for evicting a cache line
 * 
 * For example, to use a linked list based LRU,
 * you might want to have a field "struct cache_line * next" in the struct 
 */
typedef struct cache_line {
    char valid;
    mem_addr_t tag;
    int LRU;
} cache_line_t;

typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


/* The cache we are simulating */
cache_t cache;  

/* TODO - COMPLETE THIS FUNCTION
 * initCache - 
 * Allocate data structures to hold info regrading the sets and cache lines
 * use struct "cache_line_t" here
 * Initialize valid and tag field with 0s.
 * use S (= 2^s) and E while allocating the data structures here
 */
void initCache()
{
    S = pow(2,s);
    cache = malloc(S * sizeof(cache_set_t));
    if (cache == NULL) {
        printf("Cannot malloc cache.");
        exit(1);
    }

    int i;
    for (i = 0; i < S; i++) {
        *(cache + i) = malloc(E * sizeof(cache_line_t));
        if (*(cache + i) == NULL) {
            printf("Cannot malloc cache_set.");
            exit(1);
        }
        int j;
        for (j = 0; j < E; j++) {
	    (*(cache + i) + j)->valid = '0';
	    (*(cache + i) + j)->tag = 0;
    	    (*(cache + i) + j)->LRU = 0;

	}
    }
}


/* TODO - COMPLETE THIS FUNCTION 
 * freeCache - free each piece of memory you allocated using malloc 
 * inside initCache() function
 */
void freeCache()
{
    int i;
    for (i = 0; i < S; i++) {
    	free(*(cache + i));
    }

    free(cache);
}


/* TODO - COMPLETE THIS FUNCTION 
 * accessData - Access data at memory address addr.
 *   If it is already in cache, increase hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 *   you will manipulate data structures allocated in initCache() here
 */
void accessData(mem_addr_t addr)
{
    //to check whether the address's data is found
    int found = 0;

    //find the address' tag and set
    S = pow(2, s);
    B = pow(2, b);
    mem_addr_t addrTag = addr/ (B * S);
    mem_addr_t setNum = (addr / B) % S;

    //find the mostRecent and leastRecent ones
    int mostRecent;
    int leastRecent;
    int n;
    mostRecent = (*(cache + setNum) + 0)->LRU;
    leastRecent = (*(cache + setNum) + 0)->LRU;
    for (n = 0; n < E; n++) {
        if (mostRecent < (*(cache + setNum) + n)->LRU) {
            mostRecent = (*(cache + setNum) + n)->LRU;
        }
        if (leastRecent > (*(cache + setNum) + n)->LRU) {
            leastRecent = (*(cache + setNum) + n)->LRU;
        }
    }
    
    //find whether we can hit the address and change the leastRecent and mostRecent to the new one
    int i;
    for (i = 0; i < E; i++) {
    	if ((*(cache + setNum) + i)->tag == addrTag && (*(cache + setNum) + i)->valid == '1') {
	    hit_count++;
	    found = 1;
	    (*(cache + setNum) + i)->LRU = mostRecent + 1;
            mostRecent++;
            leastRecent++;
            int t;
            for (t = 0; t < E; t++) {
                if (leastRecent > (*(cache + setNum) + t)->LRU) {
                    leastRecent = (*(cache + setNum) + t)->LRU;
                }
            }
	}
    }

    //if the address is missed, try to cache it from the lower level
    if (!found) {
	miss_count++;
	
	//if the cache has space, put the address to the empty place, and change the leastRecent and mostRecent number
	int j;
	for (j = 0; j < E; j++) {
	    if ((*(cache + setNum) + j)->valid == '0') {
		(*(cache + setNum) + j)->tag = addrTag;
                (*(cache + setNum) + j)->LRU = mostRecent + 1;
                mostRecent++;
		leastRecent++;
		(*(cache + setNum) + j)->valid = '1';
                found = 1;
		int t;
		for (t = 0; t < E; t++) {
               	    if (leastRecent > (*(cache + setNum) + t)->LRU) {
                        leastRecent = (*(cache + setNum) + t)->LRU;
                    }
        	}
		break;
	    }
        }

	//if the cache is full, find the leastRecent one, and put the new one there, renew the leastRecent and mostRecent number
	if (!found) {
	    int m;
	    for (m = 0; m < E; m++) {
                if ((*(cache + setNum) + m)->LRU == leastRecent) {
		    (*(cache + setNum) + m)->tag = addrTag;
	            (*(cache + setNum) + m)->LRU = mostRecent + 1;
		    mostRecent++;
		    leastRecent++;
		    eviction_count++;
		    found = 1;
		    break;
		}
	    }
   	}
    }
}


/* TODO - FILL IN THE MISSING CODE
 * replayTrace - replays the given trace file against the cache 
 * reads the input trace file line by line
 * extracts the type of each memory access : L/S/M
 * YOU MUST TRANSLATE one "L" as a load i.e. 1 memory access
 * YOU MUST TRANSLATE one "S" as a store i.e. 1 memory access
 * YOU MUST TRANSLATE one "M" as a load followed by a store i.e. 2 memory accesses 
 */
void replayTrace(char* trace_fn)
{
    char buf[1000];
    mem_addr_t addr=0;
    unsigned int len=0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if(!trace_fp){
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while( fgets(buf, 1000, trace_fp) != NULL) {
        if(buf[1]=='S' || buf[1]=='L' || buf[1]=='M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if(verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

           // TODO - MISSING CODE
           // now you have: 
           // 1. address accessed in variable - addr 
           // 2. type of acccess(S/L/M)  in variable - buf[1] 
           // call accessData function here depending on type of access
	    if (buf[1] == 'S' || buf[1] == 'L') {
                accessData(addr);
	    }

	    if (buf[1] == 'M') {
		accessData(addr);
		accessData(addr);
	    }

            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[])
{
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }


    /* Initialize cache */
    initCache();

#ifdef DEBUG_ON
    printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file);
#endif
 
    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
