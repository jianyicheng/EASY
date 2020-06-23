/* Sparse Matrix Multiply Benchmark
 * 
 * do AxB=C
 * 
 * by Jianyi Cheng
 */

#include <stdio.h>
#include <pthread.h>
#include "sparse_matrixmult.h"
												
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int final_result = 0;

struct thread_data{
   int  startidx;
   int  maxidx;
};

void *smm (void *threadarg) {
	
	struct thread_data *arg = (struct thread_data *)threadarg;
    	int startidx = arg->startidx;
    	int maxidx = arg->maxidx;
	int i;	

	for (i = startidx; i < maxidx; i++){
        	for (int j = 0; j < COLUMN; j++){
            		for(int k = IA[i]; k < IA[i+1]; ++k) {
                		results[i][j] += A[k] * matrix_B[JA[k]][j];
            		}
        	}
    	}	

	pthread_exit(NULL);
}

int main(){

	int i, j;
    	int main_result = 0;
    	// create the thread variables
    	pthread_t threads[NUM_ACCEL];
    	struct thread_data data[NUM_ACCEL];

    	// initialize structs to pass into accels
#pragma clang loop unroll(enable) 
    	for (i = 0; i < NUM_ACCEL; i++) {
        	data[i].startidx = i * OPS_PER_ACCEL;
        	data[i].maxidx = (i + 1) * OPS_PER_ACCEL;
    	}

    	// fork the threads
#pragma clang loop unroll(enable) 
    	for (i = 0; i < NUM_ACCEL; i++) {
        	pthread_create(&threads[i], NULL, smm, (void *)&data[i]);
    	}

    	// join the threads
#pragma clang loop unroll(enable) 
    	for (i = 0; i < NUM_ACCEL; i++) {
        	pthread_join(threads[i], NULL);
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
	for (i = 0; i < COLUMN; i++){
		for (j = 0; j < ROW; j++){
			main_result += results[i][j] == matrix_C[i][j];
			//printf("expected[%d][%d] = %d, accel[%d][%d] = %d\n", y, x, expected[y][x], y, x, matrixC[y][x]);
		}
	}

	//check final result
	printf ("Result: %d\n", main_result);
	if (main_result == ROW*COLUMN) {
		printf("RESULT: PASS\n");
	} else {
		printf("RESULT: FAIL\n");
	}
#endif
	return main_result;
}
