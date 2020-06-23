#include "input.h"
#include "define.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>

#ifndef NUM_ACCEL
#define NUM_ACCEL 8
#endif

// Pthread add
void *add(void *threadarg) {
    int i, j = 0;
    int m, n;

    // get arg
    thread_arg *args = (thread_arg *) threadarg;
    int startrow = args->startrow;
    int endrow = args->endrow;

	int sum = 0;

    for (i = startrow; i < endrow; i++) {
        for (j = 0; j < WIDTH; j++) {
			sum += input[i][j];
        }
    }
	args->result = sum;

    pthread_exit(NULL);
}


int main() {
    //printf("Num threads: %d\n", NUM_ACCEL);
    int i, j, t;

    pthread_t threads[NUM_ACCEL];
    thread_arg args[NUM_ACCEL];

    unsigned int matching = 0;

    // init thread data
    for (i = 0; i < NUM_ACCEL; i++) {
        args[i].startrow = (HEIGHT/NUM_ACCEL) * i;
        args[i].endrow = (HEIGHT/NUM_ACCEL) * (i + 1);
		args[i].result = 0;
    }

    // launch threads
    for (i = 0; i < NUM_ACCEL; i++) {
        pthread_create(&threads[i], NULL, add, (void *)&args[i]);
    }

    // join threads
    for (i = 0; i < NUM_ACCEL; i++) {
        pthread_join(threads[i], NULL);
    }

//#define SW 1
#ifdef SW
    int final_result = 0;
    for (i = 0; i < NUM_ACCEL; i++) {
        final_result += args[i].result;
    }
    printf("SUM: %d\n", final_result);
#else
    // get result
    int final_result = 0;
    for (i = 0; i < NUM_ACCEL; i++) {
        final_result += args[i].result;
    }

    printf("SUM: %d, EXPECTED %d\n", final_result, 816976);

    if (final_result == 816976) {
        printf("RESULT: PASS\n");
    } else {
        printf("RESULT: FAIL\n");
    }
#endif

    return 0;
}
