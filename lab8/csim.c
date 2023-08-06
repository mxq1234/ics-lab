/*
 * MaXiaoqian 520030910288
 *
 * csim.c - Cache Simulator
 * 
 * A cache simulator that takes a valgrind memory trace as input,
 * simulates the hit/miss behavior of a cache memory on this trace,
 * and outputs the total number of hits, misses and evictions.
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

/* define the data structure of Cache */
typedef struct {
    int valid;
    int timeStamp;
    int tag;
} Line;

typedef struct {
    Line* lines;
    int maxTimeStamp;
} Set;

typedef struct {
    int setNum;
    int lineNum;
    int blockBit;
    Set* sets;
} Cache;

int hits;
int misses;
int evictions;


/*
 *  Func: malloc for a new Cache
 *  Param: s is setBits, E is lineNum, b is blockBit
 *  Return: the pointer of new Cache
 *  Error: when the paraments <= 0, or malloc error
 */
Cache* new_Cache(int s, int E, int b)
{
    if(s <= 0 || E <= 0 || b <= 0) {
        printf("Invalid params for the cache");
        exit(1);
    }

    Cache* cache = (Cache*)malloc(sizeof(Cache));
    if(!cache) {
        printf("Can't malloc for cache");
        exit(1);
    }

    int S = (1 << s);
    cache->setNum = S;
    cache->lineNum = E;
    cache->blockBit = b;

    Set* sets = cache->sets = (Set*)malloc(sizeof(Set) * S);
    if(!sets) {
        printf("Can't malloc for cache sets");
        exit(1);
    }

    for(int i = 0; i < S; ++i) {
        Line* lines = (Line*)malloc(sizeof(Line) * E);
        if(!lines) {
            printf("Can't malloc for cache lines");
            exit(1);
        }

        sets[i].lines = lines;
        sets[i].maxTimeStamp = 0;
        for(int j = 0; j < E; ++j) {
            lines[j].valid = 0;
            lines[j].timeStamp = 0;
        }
    }

    return cache;
}

/*
 *  Func: free the Cache memory pointed by the pointer
 *  Param: a pointer point to the Cache memory to be free
 *  Error: null pointer or valid pointer
 */
void delete_Cache(Cache* cache)
{
    if(!cache) {
        printf("Can't free the cache using null pointer");
        exit(1);
    }

    Set* sets = cache->sets;
    int S = cache->setNum;
    for(int i = 0; i < S; ++i)
        free(sets[i].lines);
    free(sets);
    free(cache);
}

/*
 *  Func: print the usage of the command line
 */
void usage()
{
    printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
    exit(0);
}

/*
 *  Func: check if the param after option exists
 *  Param: optarg pointer
 */
void paramsExist(char* optarg)
{
    if(optarg[0] == '-') {
        printf("Required params missing!");
        exit(1);
    }
}

/*
 *  Func: parse the command line with multiple options for Cache
 *  Param: argc,argv is the paraments of main function
 *         s,E,b,filename is the paraments after -s,-E,-b,-t
 *  Return: whether to enable verbose output
 */
int getOpt(int argc, char** argv, int* s, int* E, int* b, char* fileName)
{
    int option, verbose = 0;
    while((option = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch(option) {
        case 'v': verbose = 1; break;
        case 's': paramsExist(optarg); *s = atoi(optarg); break;
        case 'E': paramsExist(optarg); *E = atoi(optarg); break;
        case 'b': paramsExist(optarg); *b = atoi(optarg); break;
        case 't': strcpy(fileName, optarg); break;
        case 'h':
        default: usage();
        }
    }
    return verbose;
}

/*
 *  Func: find whether it hits
 *  Param: E is lineNum, tag is targetTag, maxTimeStamp is used to
 *         update the timeStamp
 *  Return: whether it hits
 */
int isHit(Line* lines, int E, int tag, int* maxTimeStamp)
{
    for(int i = 0; i < E; ++i) {
        if(lines[i].valid && lines[i].tag == tag) {
            lines[i].timeStamp = ++(*maxTimeStamp);
            hits++;
            return 1;
        }
    }
    return 0;
}

/*
 *  Func: find the most suitable line to load the target data
 *  Param: E is lineNum, tag is targetTag, maxTimeStamp is used to
 *         update the timeStamp
 *  Return: whether evict line
 */
int handleMiss(Line* lines, int E, int tag, int* maxTimeStamp)
{
    misses++;
    int minTimeStamp = 0x7fffffff, evictTarget = 0;
    for(int i = 0; i < E; ++i) {
        if(!lines[i].valid) {
            lines[i].valid = 1;
            lines[i].tag = tag;
            lines[i].timeStamp = ++(*maxTimeStamp);
            return 0;
        }
        if(lines[i].timeStamp < minTimeStamp) {
            minTimeStamp = lines[i].timeStamp;
            evictTarget = i;
        }
    }

    /* Evict the evictTarget line */
    evictions++;
    lines[evictTarget].tag = tag;
    lines[evictTarget].timeStamp = ++(*maxTimeStamp);
    return 1;
}

/*
 *  Func: access the data int the addr, and change the hits, misses,
 *        evictions if it hits, misses or evicts
 *  Param: cache is the pointer of Cache Simulator
 *         addr is the address of the accessed data
 *         verbose is whether to enable verbose output
 */
void accessData(Cache* cache, int addr, int verbose)
{
    /* Parse the data into targetSet and tag */
    int b = cache->blockBit, s = 0;
    for(int S = cache->setNum; S > 1; S >>= 1, ++s);
    int targetSet = ((addr >> b) & ((1 << s) - 1));
    int tag = (addr >> (s + b));

    /* Reduction in Strength */
    Line* lines = cache->sets[targetSet].lines;
    int E = cache->lineNum;
    int maxTimeStamp = cache->sets[targetSet].maxTimeStamp;

    if(isHit(lines, E, tag, &maxTimeStamp)) {
        if(verbose)   printf(" hit");
    } else {
        if(verbose)   printf(" miss");
        if(handleMiss(lines, E, tag, &maxTimeStamp))
            if(verbose)   printf(" eviction");
    }
    if(verbose)  printf("\n");

    /* Avoid timeStamp overflow */
    if(maxTimeStamp >= 0x7fffffff) {
        for(int i = 0; i < E; ++i)
            lines[i].timeStamp -= 0x7fffffff;
    }
    cache->sets[targetSet].maxTimeStamp = maxTimeStamp;
}


int main(int argc, char** argv)
{
    int s = 0, E = 0, b = 0;
    char fileName[100];
    int verbose = getOpt(argc, argv, &s, &E, &b, fileName);

    FILE* fin = fopen(fileName, "r");
    if(!fin) {
        printf("Can't open file %s", fileName);
        exit(1);
    }

    Cache* cache = new_Cache(s, E, b);
    char operation[2];
    int addr = 0, size = 0;
    hits = misses = evictions = 0;

    while(fscanf(fin, "%s %x,%d\n", operation, &addr, &size) != EOF) {
        if(operation[0] == 'I')    continue;

        if(verbose)  printf("%s %x,%d", operation, addr, size);
        switch(operation[0]) {
        case 'M': accessData(cache, addr, verbose);
        case 'L':
        case 'S': accessData(cache, addr, verbose);
        }
    }
    fclose(fin); 
    printSummary(hits, misses, evictions);

    delete_Cache(cache);   
    return 0;
}
