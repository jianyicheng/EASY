/* Parallelizable Matrix Multiply Benchmark
 * 
 * do AxB=C
 * 
 * by Kevin Nam
 */
#include <stdio.h>
#include <pthread.h>
#include "matrixmult.h"
												
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int final_result = 0;

struct thread_data{
   int  startidx;
   int  maxidx;
};

// Calculate the values for an entire row
//void multiply (int y, int INPUTA_MATRIXDIMENSION_X){
void *multiply (void *threadarg) {
	
	int i,j,k;
	int y = *((int *)threadarg);
	int result = 0;
	for (k = y; k < y + INPUTA_MATRIXDIMENSION_Y/NUM_ACCEL; k++) {
		for (j = 0; j < INPUTB_MATRIXDIMENSION_X; j++){
			//calculating the multiplication
			for (i = 0; i < INPUTA_MATRIXDIMENSION_X; i++) {
				result+= matrixA[k][i]*matrixB[i][j];
			}
			//writing back the result
			matrixC[k][j] = result;
			result = 0;
		}
	}
	pthread_exit(NULL);
}

int main(){

	int x = 0, y = 0;
	int main_result=0;
	//create the thread variables
	pthread_t threads[NUM_ACCEL];
 	int data[NUM_ACCEL];

	//prepare data
	for (y = 0; y<NUM_ACCEL; y++) {
		data[y]=y * INPUTA_MATRIXDIMENSION_Y/NUM_ACCEL;
	}

	// Do the multiplies (row at a time)
	for (y = 0; y<NUM_ACCEL; y++) {
		pthread_create(&threads[y], NULL, multiply, (void *)&data[y]);
	}

	//join the threads
	for (y=0; y<NUM_ACCEL; y++) {
		pthread_join(threads[y], NULL);
	}

//#define SW_TEST 1
#ifdef SW_TEST
	printf("{");
	for (y = 0; y < OUTPUT_MATRIXDIMENSION_Y; y++){
		printf("{");
		for (x = 0; x < OUTPUT_MATRIXDIMENSION_X; x++){
			if (y == OUTPUT_MATRIXDIMENSION_Y - 1 && x == OUTPUT_MATRIXDIMENSION_X - 1) {
				printf("%d}", matrixC[y][x]);
			} else if (x == OUTPUT_MATRIXDIMENSION_X - 1) {
				printf("%d},\n", matrixC[y][x]);
			} else {
				printf("%d,", matrixC[y][x]);
			}
		}
	}
	printf("}\n");
#else
	// check result
	for (y = 0; y < OUTPUT_MATRIXDIMENSION_Y; y++){
		for (x = 0; x < OUTPUT_MATRIXDIMENSION_X; x++){
			main_result += expected[y][x] == matrixC[y][x];
			//printf("expected[%d][%d] = %d, accel[%d][%d] = %d\n", y, x, expected[y][x], y, x, matrixC[y][x]);
		}
	}

	//check final result
	printf ("Result: %d\n", main_result);
	if (main_result == OUTPUT_MATRIXDIMENSION_X*OUTPUT_MATRIXDIMENSION_Y) {
		printf("RESULT: PASS\n");
	} else {
		printf("RESULT: FAIL\n");
	}
#endif
	return main_result;
}

