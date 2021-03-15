/** 
2  
3 Author: Ananya Kumar 
4 Organization: Carnegie Mellon University 
5 Description: Cache Simulator 
6 Notes: We assume that the cache size does not exceed 2^30 bytes ~ 1GB 
7  
8 **/ 
9 
 
10 #include <stdio.h> 
11 #include <unistd.h> 
12 #include <getopt.h> 
13 #include <stdbool.h> 
14 #include <string.h> 
15 #include <stdlib.h> 
16 #include "contracts.h" 
17 #include "cachelab.h" 
18 
 
19 /* Explains to the user how they should use the function */ 
20 void printUsage () 
21 { 
22 	fprintf(stderr, "Usage: ./csim  [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n"); 
23 	fprintf(stderr, "Options:\n" 
24  				 	  "  -h         Print this help message.\n" 
25 					  "  -v         Optional verbose flag.\n" 
26 					  "  -s <num>   Number of set index bits.\n" 
27 					  "  -E <num>   Number of lines per set.\n" 
28 					  "  -b <num>   Number of block offset bits.\n" 
29 					  "  -t <file>  Trace file.\n\n" 
30 
 
31 					"Examples:\n" 
32 					  "  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n" 
33 					  "  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n"); 
34 } 
35 
 
36 /* malloc but exits on failure */ 
37 void *xmalloc (size_t len) 
38 { 
39 	void *ptr = malloc(len); 
40 
 
41 	if (ptr == NULL) { 
42 		fprintf(stderr, "Could not allocate memory.\n"); 
43 		exit(1); 
44 	}  
45 
 
46 	else return ptr; 
47 } 
48 
 
49 /* calloc but exits on failure */ 
50 void *xcalloc (size_t num, size_t size) 
51 { 
52 	void *ptr = calloc(num, size); 
53 
 
54 	if (ptr == NULL) { 
55 		fprintf(stderr, "Could not allocate memory.\n"); 
56 		exit(1); 
57 	}  
58 
 
59 	else return ptr; 
60 } 
61 
 
62 /** Returns unsigned 64-bit int with lowest i bits set to 1 */ 
63 unsigned long long int mask (int i) 
64 { 
65 	REQUIRES(0 <= i && i <= 64); 
66 	unsigned long long int m = 1; 
67 	return (m<<i)-1; 
68 } 
69 
 
70 int main (int argc, char* argv[]) 
71 { 
72 	int opt; 
73 	int s = -1, E = -1, b = -1; 
74 	char *fileName = NULL; 
75 	bool verbose = false; 
76 	int i; //Loop variables 
77 
 
78 	/** Parse command line arguments **/ 
79 
 
80 	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) 
81 	{ 
82 		switch (opt)  
83 		{ 
84 			case 'v': 
85 				verbose = true; 
86 				break; 
87 			case 's': 
88 				s = atoi(optarg); 
89 				break; 
90 			case 'E': 
91 				E = atoi(optarg); 
92 				break; 
93 			case 'b': 
94 				b = atoi(optarg); 
95 				break; 
96 			case 't': 
97 				fileName = xmalloc((strlen(optarg)+1) * sizeof(char)); 
98         strcpy(fileName, optarg); 
99 				break; 
100 			case 'h': 
101 			default: 
102 				printUsage(); 
103 				exit(0); 
104 		} 
105 	} 
106 
 
107 	if (s < 1 || E < 1 || b < 1 || s > 30 || E > 30 || !fileName) { 
108 		printUsage(); 
109 		exit(0); 
110 	} 
111 
 
112 	FILE *tracePtr = fopen(fileName, "r"); 
113 
 
114 	if (!tracePtr) { 
115 		printUsage(); 
116 		exit(0); 
117 	} 
118 
 
119 	/** Set up cache data structures **/ 
120 
 
121 	int sets = 1 << s; //Assume we won't have >= 2^31 sets 
122 
 
123 	unsigned long long int **cacheTag = xmalloc(sets *  
124                                               sizeof(unsigned long long int *)); 
125 	//cacheTag[i][j] stores the tag in the j^th line of the i^th set 
126 
 
127 	unsigned long long int **timeStamp = xmalloc(sets *  
128                                               sizeof(unsigned long long int *)); 
129 	//timeStamp[i][j] stores the time when the j^th line was added to the i^th set 
130 
 
131 	for (i = 0; i < sets; i++) 
132 	{ 
133 		cacheTag[i] = xmalloc(E * sizeof(unsigned long long int)); 
134 		timeStamp[i] = xcalloc(E, sizeof(unsigned long long int)); 
135 	} 
136 
 
137 	/** Simulate Cache **/ 
138 
 
139 	char line[50]; 
140 	int curTime = 1; 
141 	int misses = 0, hits = 0, evictions = 0; 
142 
 
143 	while (fgets(line, 50, tracePtr) != NULL)  
144 	{ 
145 		unsigned long long int address; 
146 		int size; 
147 		char op; 
148 
 
149 		sscanf(line, " %c %llx,%d", &op, &address, &size); 
150 		if (verbose) printf("%c %llx,%d ", op, address, size); 
151 
 
152 		if (op == 'I') continue; //Ignore instruction operations 
153 
 
154 		unsigned long long int setIndex = (address>>b) & mask(s); //Extract set 
155 		unsigned long long int *curSetTag = cacheTag[setIndex]; 
156 		unsigned long long int *curSetStamp = timeStamp[setIndex]; 
157 
 
158 		unsigned long long int tag = (address>>(b+s)) & mask(64-b-s); //Extract tag 
159 
 
160 		//Search lines of appropriate set to check if address is in cache 
161 
 
162 		int tagIdx = -1; //Which line of cache is the current address in? 
163 
 
164 		for (i = 0; i < E; i++)  
165 		{ 
166 			if (curSetStamp[i] != 0 && curSetTag[i] == tag) { 
167 				tagIdx = i; 
168 				break; 
169 			} 
170 		} 
171 
 
172 		//If the memory block is in cache, we have a hit! 
173 
 
174 		if (tagIdx != -1) { 
175 			curSetStamp[tagIdx] = curTime; //Update time for LRU algorithm 
176 			hits++; 
177 			if (verbose) printf("Hit "); 
178 		} 
179 
 
180 		//Otherwise, we need to store memory block in cache 
181 
 
182 		else { 
183 
 
184 			misses++; 
185 			if (verbose) printf("Miss "); 
186 
 
187 			//Find LRU 
188 
 
189 			int lruIdx = 0; //Address of least recently used line 
190 
 
191 			for (i = 0; i < E; i++) 
192 			{ 
193 				if (curSetStamp[i] <= curSetStamp[lruIdx]) { 
194 					lruIdx = i; 
195 				} 
196 			} 
197 
 
198 			if (curSetStamp[lruIdx]) { 
199 				evictions++; 
200 				if (verbose) printf("Evicted "); 
201 			}  
202 
 
203 			curSetTag[lruIdx] = tag; 
204 			curSetStamp[lruIdx] = curTime; 
205 		} 
206 
 
207 		//The 2nd step of a modify operation is always a hit 
208 
 
209 		if (op == 'M') { 
210 			hits++; 
211 			if (verbose) printf("Hit "); 
212 		} 
213 
 
214 		curTime++; 
215 		if (verbose) printf("\n"); 
216 	} 
217 
 
218   //Free allocated data 
219    
220   for (i = 0; i < sets; i++) 
221   { 
222     free(cacheTag[i]); 
223     free(timeStamp[i]); 
224   } 
225 
 
226   free(cacheTag); 
227   free(timeStamp); 
228   free(fileName); 
229   fclose(tracePtr); 
230 
 
231 	//print final statistics 
232 	printSummary(hits, misses, evictions); 
233 	return 0; 
234 } 
