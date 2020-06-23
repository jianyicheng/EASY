//Two parallel functions for this benchmark: 1. swap, which transposes the matrix, 2. check, which checks the output matrix with the expected. 

#define INPUT_ROWS 128
#define INPUT_COLS 128
#ifndef NUM_ACCEL
#define NUM_ACCEL 8
#endif

#define OUTPUT_ROWS INPUT_COLS
#define OUTPUT_COLS INPUT_ROWS

#include <stdio.h>
#include <pthread.h>
#include "matrixtrans.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int final_result = 0;

void *swap (void *threadarg) {
	int y, k;
	int checksum=0;
	int accel_num = *((int *)threadarg);
	for (k=accel_num; k<INPUT_COLS; k+=NUM_ACCEL) {
		for (y=0; y<INPUT_ROWS; y++) {
			output[k][y] = input_array[y][k];		
		}
	}

	pthread_exit(NULL);
}


int main() {

	int i,j;
	int main_result=0;
	//create the thread variables
	pthread_t threads[NUM_ACCEL];
 	int data[NUM_ACCEL];

	//Perform the Matrix Transpose using threads in a cyclic manner
	for (i=0; i<NUM_ACCEL; i++) {
		data[i]=i;
		pthread_create(&threads[i], NULL, swap, (void *)&data[i]);
	}

	//join the threads
	for (i=0; i<NUM_ACCEL; i++) {
		pthread_join(threads[i], NULL);
	}

//#define SW 1
#ifdef SW
	//generate the sw output
	printf("{\n");
	for (i=0; i<OUTPUT_ROWS; i++) {
		printf("{");
		for (j=0; j<OUTPUT_COLS; j++) {
			if (j == OUTPUT_COLS-1) {
				printf(" %d", output[i][j]);
			} else {
				printf(" %d,", output[i][j]);
			}
		}
		if (i == OUTPUT_ROWS-1) {
			printf("}\n");
		} else {
			printf("},\n");
		}
	}
	printf("}\n");
#else // check output
	//check the output
	for (i=0; i<OUTPUT_ROWS; i++) {
		for (j=0; j<OUTPUT_COLS; j++) {
			final_result += (expected_array[i][j] == output[i][j]);
		}
	}

	//check final result
	printf ("Result: %d\n", final_result);
	if (final_result == 524288) {
		printf("RESULT: PASS\n");
	} else {
		printf("RESULT: FAIL\n");
	}
#endif

	return 0;
}
