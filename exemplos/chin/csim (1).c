/* 
2  * csim.c - A cache simulator that can replay traces from Valgrind 
3  *     and output statistics such as number of hits, misses, and 
4  *     evictions.  The replacement policy is LRU. 
5  * 
6  * Implementation and assumptions: 
7  *  1. Each load/store can cause at most one cache miss. (I examined the trace, 
8  *  the largest request I saw was for 8 bytes). 
9  *  2. Instruction loads (I) are ignored, since we are interested in evaluating 
10  *  trans.c in terms of its data cache performance. 
11  *  3. data modify (M) is treated as a load followed by a store to the same 
12  *  address. Hence, an M operation can result in two cache hits, or a miss and a 
13  *  hit plus an possible eviction. 
14  * 
15  * The function printSummary() is given to print output. 
16  * Please use this function to print the number of hits, misses and evictions. 
17  * This is crucial for the driver to evaluate your work. 
18  */ 
19 #include <getopt.h> 
20 #include <stdlib.h> 
21 #include <unistd.h> 
22 #include <stdio.h> 
23 #include <assert.h> 
24 #include <math.h> 
25 #include <limits.h> 
26 #include <string.h> 
27 #include <errno.h> 
28 #include "cachelab.h" 
29 
 
30 // #define DEBUG_ON 
31 #define ADDRESS_LENGTH 64 
32 
 
33 /* Type: Memory address */ 
34 typedef unsigned long long int mem_addr_t; 
35 
 
36 /* Type: Cache line 
37    LRU is a counter used to implement LRU replacement policy  */ 
38 typedef struct cache_line { 
39     char valid; 
40     mem_addr_t tag; 
41     unsigned long long int lru; 
42 } cache_line_t; 
43 
 
44 typedef cache_line_t *cache_set_t; 
45 typedef cache_set_t *cache_t; 
46 
 
47 /* Globals set by command line args */ 
48 int verbosity = 0; /* print trace if set */ 
49 int s = 0; /* set index bits */ 
50 int b = 0; /* block offset bits */ 
51 int E = 0; /* associativity */ 
52 char *trace_file = NULL; 
53 
 
54 /* Derived from command line args */ 
55 int S; /* number of sets */ 
56 int B; /* block size (bytes) */ 
57 
 
58 /* Counters used to record cache statistics */ 
59 int miss_count = 0; 
60 int hit_count = 0; 
61 int eviction_count = 0; 
62 unsigned long long int lru_counter = 1; 
63 
 
64 /* The cache we are simulating */ 
65 cache_t cache; 
66 mem_addr_t set_index_mask; 
67 
 
68 /* 
69  * initCache - Allocate memory, write 0's for valid and tag and LRU 
70  * also computes the set_index_mask 
71  */ 
72 void initCache() { 
73     int i, j; 
74     cache = (cache_set_t *) malloc(sizeof(cache_set_t) * S); 
75     for (i = 0; i < S; i++) { 
76         cache[i] = (cache_line_t *) malloc(sizeof(cache_line_t) * E); 
77         for (j = 0; j < E; j++) { 
78             cache[i][j].valid = 0; 
79             cache[i][j].tag = 0; 
80             cache[i][j].lru = 0; 
81         } 
82     } 
83 
 
84     /* Computes set index mask */ 
85     set_index_mask = (mem_addr_t) (pow(2, s) - 1); 
86 } 
87 
 
88 
 
89 /* 
90  * freeCache - free allocated memory 
91  */ 
92 void freeCache() { 
93     int i; 
94     for (i = 0; i < S; ++i) 
95         free(cache[i]); 
96     free(cache); 
97 } 
98 
 
99 
 
100 /* 
101  * accessData - Access data at memory address addr. 
102  *   If it is already in cache, increast hit_count 
103  *   If it is not in cache, bring it in cache, increase miss count. 
104  *   Also increase eviction_count if a line is evicted. 
105  */ 
106 void accessData(mem_addr_t addr) { 
107     int i; 
108     /* unsigned long long int eviction_lru = ULONG_MAX; */ 
109     /* unsigned int eviction_line = 0; */ 
110     mem_addr_t set_index = (addr >> b) & set_index_mask; 
111     mem_addr_t tag = addr >> (s + b); 
112 
 
113     cache_set_t cache_set = cache[set_index]; 
114 
 
115     /* judge if hit */ 
116     for (i = 0; i < E; ++i) { 
117         /* hit */ 
118         if (cache_set[i].valid && cache_set[i].tag == tag) { 
119             if (verbosity) 
120                 printf("hit "); 
121             ++hit_count; 
122 
 
123             /* update entry whose lru is less than the current lru (newer) */ 
124             for (int j = 0; j < E; ++j) 
125                 if (cache_set[j].valid && cache_set[j].lru < cache_set[i].lru) 
126                     ++cache_set[j].lru; 
127             cache_set[i].lru = 0; 
128             return; 
129         } 
130     } 
131 
 
132     /* missed */ 
133     if (verbosity) 
134         printf("miss "); 
135     ++miss_count; 
136 
 
137     /* find the biggest lru or the first invlaid line */ 
138     int j, maxIndex = 0; 
139     unsigned long long maxLru = 0; 
140     for (j = 0; j < E && cache_set[j].valid; ++j) { 
141         if (cache_set[j].lru >= maxLru) { 
142             maxLru = cache_set[j].lru; 
143             maxIndex = j; 
144         } 
145     } 
146 
 
147     if (j != E) { 
148         /* found an invalid entry */ 
149         /* update other entries */ 
150         for (int k = 0; k < E; ++k) 
151             if (cache_set[k].valid) 
152                 ++cache_set[k].lru; 
153         /* insert line */ 
154         cache_set[j].lru = 0; 
155         cache_set[j].valid = 1; 
156         cache_set[j].tag = tag; 
157     } else { 
158         /* all entry is valid, replace the oldest one*/ 
159         if (verbosity) 
160             printf("eviction "); 
161         ++eviction_count; 
162         for (int k = 0; k < E; ++k) 
163             ++cache_set[k].lru; 
164         cache_set[maxIndex].lru = 0; 
165         cache_set[maxIndex].tag = tag; 
166     } 
167 } 
168 
 
169 
 
170 /* 
171  * replayTrace - replays the given trace file against the cache 
172  */ 
173 void replayTrace(char *trace_fn) { 
174     char buf[1000]; 
175     mem_addr_t addr = 0; 
176     unsigned int len = 0; 
177     FILE *trace_fp = fopen(trace_fn, "r"); 
178 
 
179     int testcount = 0; 
180 
 
181     while (fscanf(trace_fp, " %c %llx,%d", buf, &addr, &len) > 0) { 
182         if (verbosity && buf[0] != 'I') 
183             printf("%c %llx,%d ", buf[0], addr, len); 
184         switch (buf[0]) { 
185             case 'I': 
186                 break; 
187             case 'L': 
188             case 'S': 
189                 accessData(addr); 
190                 ++testcount; 
191                 break; 
192             case 'M': 
193                 accessData(addr); 
194                 accessData(addr); 
195                 ++testcount; 
196                 break; 
197             default: 
198                 break; 
199         } 
200         if (verbosity && buf[0] != 'I') 
201             putchar('\n'); 
202     } 
203     fclose(trace_fp); 
204 } 
205 
 
206 void debug() { 
207     printf("========================\n"); 
208     for (int i = 0; i < S; ++i) { 
209         for (int j = 0; j < E; ++j) 
210             printf("%d,%10llx,%4llu | ", cache[i][j].valid, cache[i][j].tag, cache[i][j].lru); 
211         printf("\n"); 
212     } 
213 } 
214 
 
215 /* 
216  * printUsage - Print usage info 
217  */ 
218 void printUsage(char *argv[]) { 
219     printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]); 
220     printf("Options:\n"); 
221     printf("  -h         Print this help message.\n"); 
222     printf("  -v         Optional verbose flag.\n"); 
223     printf("  -s <num>   Number of set index bits.\n"); 
224     printf("  -E <num>   Number of lines per set.\n"); 
225     printf("  -b <num>   Number of block offset bits.\n"); 
226     printf("  -t <file>  Trace file.\n"); 
227     printf("\nExamples:\n"); 
228     printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]); 
229     printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]); 
230     exit(0); 
231 } 
232 
 
233 /* 
234  * main - Main routine 
235  */ 
236 int main(int argc, char *argv[]) { 
237     char c; 
238 
 
239     while ( (c = getopt(argc, argv, "s:E:b:t:vh")) != -1) { 
240         switch (c) { 
241             case 's': 
242                 s = atoi(optarg); 
243                 break; 
244             case 'E': 
245                 E = atoi(optarg); 
246                 break; 
247             case 'b': 
248                 b = atoi(optarg); 
249                 break; 
250             case 't': 
251                 trace_file = optarg; 
252                 break; 
253             case 'v': 
254                 verbosity = 1; 
255                 break; 
256             case 'h': 
257                 printUsage(argv); 
258                 exit(0); 
259             default: 
260                 printUsage(argv); 
261                 exit(1); 
262         } 
263     } 
264 
 
265     /* Make sure that all required command line args were specified */ 
266     if (s == 0 || E == 0 || b == 0 || trace_file == NULL) { 
267         printf("%s: Missing required command line argument\n", argv[0]); 
268         printUsage(argv); 
269         exit(1); 
270     } 
271 
 
272     /* Compute S, E and B from command line args */ 
273     S = (unsigned int) pow(2, s); 
274     B = (unsigned int) pow(2, b); 
275 
 
276     /* Initialize cache */ 
277     initCache(); 
278 
 
279 #ifdef DEBUG_ON 
280     printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file); 
281     printf("DEBUG: set_index_mask: %llu\n", set_index_mask); 
282 #endif 
283 
 
284     replayTrace(trace_file); 
285 
 
286     /* Free allocated memory */ 
287     freeCache(); 
288 
 
289     /* Output the hit and miss statistics for the autograder */ 
290     printSummary(hit_count, miss_count, eviction_count); 
291     return 0; 
292 } 
