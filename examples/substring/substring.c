// This algorithm implements naive substring matching
// The algorithm is described on Wiki

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef NUM_ACCEL
#define NUM_ACCEL 4
#endif

#define LEN 2048
#define SLEN 3

char str[] = "aaaacaaacccagagagtcaaaagaagatcaaaaagttagggttcaagagccggagatggcgcgattattacggcggcgcgcgagagagagctcgcgaaagggggggttttttttttatttattattgtgaggtggcgcgcctctcctccaagactatagatatagagaaggatcgcgcattatagcgcgctttcgggcaaaataagacaaactctcgatatatagcctgcgcatctcttctctcagagtctctcaggcgcgctaccctatcttattgcgcgcatatcttctcgaagtctcaggagatctctcagagtctcttctcgctctcgagaggatcctagagagagctctctagagatcgcgcatatagatagatcgagatagagactgagataagagagagactaggagagagagagagaggggggcgcggcggagggcagagaaatatatataaactctatatatatctatataattatgatatagatatatagatataagatatatatagagagatctcattactactactgactgatcgtgtgctagctagctagctagctagctagctagctagctacatcatatcgcgcgcccgcgcccagagacaaacatatgaacataggacataggctagatagatcgatgagctagagatcgatagatcgcgatagctaggacggcgatagctctagagaaaatcgagatcgagaccgatcgatcgataggagatcccgagaccaacaaaaatagaccagagagatcgcgcgcggcgggcggctctttctagggattgagtagcggatcgatcgggatagccgatcgatatatttattctactgaggagctagctagagatctatagattcccaaaaccggggaaacgttggcggggaggggcgggaggcggctttgtggggtcggcggcggagagagtcgcggctagagagctgcgcgatgcgcgcgcgatatagagcgcgatatagagagagagcgcgcgcgcgcatatatatatatatattataatttcctatttcggggatcgggagagagagaaagcggaggcggggatatatagggagagagagatctctgcgcgatataaatacggatagcgatatagaaaaagcgggcagaagcgggcgagaaaaaacggccttcggagcgcgagagagcgggattaggggaggaggggcggggcggaggtgggcgcgatatatatatatatatacccgccgcgcgcgatatagagagagagaatataaaaaaaatataaaataaagataagacgatcgatcgatcgatagagagagagatcgcgaggagatatagagataaacccgcgcgcgcggggggggttttttctctcccccccaagagaatcgatagctaccaaagtgggggagggcggggccccctcaggagagctcgatcggcgatggcgctagcgcttcgcgatctgcgcgcgcgcccccctctctcttctcttttcgcgcggattatcagggaaaaaaagaggaaaaaatccaggctaaacgcgccccgaggcccccccccaacaaccagggctttggtcaacgtttggtgaggggcgagggttggaggtggtgaggcccagggaggcttttttggagagatctggatctgaggtcggaggatagggacccccccgctctcgcgcgtctcgcgcgccgcgggtttaatagcttagctaggatagagagatagattgtttgtgctgctgctgattcgtctctctctgctgctgctgctgcaagagagagatctctgagagatctggatcgcgcactcgcgtaggctagcgtcgaggctctcgagctcgagctctaggatatgagctctgagagtcgaacacccaccggtgtgtgaaaagggggaaaaccaaccaacaacaccacctcctgcggcccgccacaacccaggtattaagaaagattagccacgacccgaccttgacgacgttgaccgtagcgttgacgatgacccgatgaccgacgttgtgtagcccaccaaacgcga";

typedef struct {
	int start;
} thread_arg;

// Pthread substring search
void *substrMatch (void *arg) {
	//int len = strlen(str);
	//int slen = strlen(substr);
	int len = LEN;
	int slen = SLEN;
	thread_arg *targ = (thread_arg *)arg;
	int startidx = targ->start;
	int endidx = startidx + (LEN/NUM_ACCEL);
	if (endidx > len) {
		endidx = len;
	}
	
	//printf("thread start %d\n", startidx);
	int num_match = 0;
	int i;

	//printf("startidx %d, len %d, slen %d\n", startidx, len, slen);
	char substr[] = "aac";
	//char substr[] = {'a', 'a', 'c', '\0'};

	for (i = startidx; i < endidx; i++) {
		int j = 0;
		bool match = true;
		for (j = 0; j < slen; j++) {
			if (str[i+j] != substr[j]) {
				match = false;
			}
		}
		if (match == true) {
			num_match++;
		}
	}
	//printf("starting idx %d, found matches %d\n", startidx, num_match);

	pthread_exit((void *)num_match);
}

int main () {
	int i;
	pthread_t threads[NUM_ACCEL];
	thread_arg threadargs[NUM_ACCEL];
	int result[NUM_ACCEL];
	int sum = 0;

	// create threads to perform search
	for (i = 0; i < NUM_ACCEL; i++) {
		threadargs[i].start = i * (LEN/NUM_ACCEL);
		pthread_create(&threads[i], NULL, substrMatch, (void *)&threadargs[i]);
	}

	// join the threads and get return value
	for (i = 0; i < NUM_ACCEL; i++) {
		pthread_join(threads[i], (void **)&result[i]);
	}

	// sum the number of matches
	for (i = 0; i < NUM_ACCEL; i++) {
		sum += result[i];
	}

	printf("MATCHES: %d\n", sum);

	if (sum == 22) {
		printf("RESULT: PASS\n");
	} else {
		printf("RESULT: FAIL\n");
	}

	return 0;
}

