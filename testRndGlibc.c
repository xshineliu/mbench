// gcc -o tstRnd -O0 testRndGlibc.c -fopenmp


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <omp.h>
#include <unistd.h>
 
#define MAX_CORES               4096
#define DEFAULT_LOOP    (100*1000*1000)

long long unsigned start_ns;
struct timespec ts;
//int cnt[MAX_CORES];

//volatile int token  __attribute__((aligned(0x1000)));

static inline long long unsigned time_ns(struct timespec* const ts) {
        if (clock_gettime(CLOCK_REALTIME, ts)) {
                exit(1);
        }
        return ((long long unsigned) ts->tv_sec) * 1000000000LLU
                        + (long long unsigned) ts->tv_nsec;
}

void *counting(void *param)
{
        unsigned long long cnt = (unsigned long long) param;
    	void *base = (void *)0;
    	asm volatile ("1:\n\t"
		"mov %0, %%rax\n\t"
		"dec %2\n\t"
		"lea 0x1(%%rax), %0\n\t"
		"test %2, %2\n\t"
		"jne 1b\n\t"
		:"=r"(base):"r"(base), "r"(cnt)
		: "%rax", "memory");
	return base;
}

int measure(long long loops, long long delay) {
        long long unsigned delta;
        int idx = 0;
        int rand = 0;
        long long i = 0;
        int n_threads = 1; // omp_get_num_procs();  & omp_get_num_threads() doesn't work;

        token = 0;

#pragma omp parallel
        {
                n_threads = omp_get_num_threads();
                printf("#### %04d/%04d\n", omp_get_thread_num(), n_threads);

        }

          start_ns = time_ns(&ts);

#pragma omp parallel private(idx, rand, i)
        {
                idx = omp_get_thread_num();
	        	srand(time(0) | idx );
                for(i = 0; i < loops; i++) {
                       rand += random();
		            	if(delay > 0) {
			                	rand += (intptr_t)counting((void *)(intptr_t)delay);
		            	}
                }

        }

        delta = time_ns(&ts) - start_ns;
        printf("RES: TIME_ALL %.06f PER_LOOP %.03f BW %.03f\n", (double)delta / 1000000.0f, (double)delta / (double)loops, (double)n_threads * (double)loops * 1000.0f / (double)delta);
        return rand;
}

int main(int argc, char *argv[]) {

        long long loops = 0;
    	long long delay = 0;
        int ret = 0;
        if(argc > 1) {
                loops = atoll(argv[1]);
        }

        if(argc > 2) {
                delay = atoll(argv[2]);
        }
 
        if(loops < 1) {
                loops = DEFAULT_LOOP;
        }
        printf("### Use loops = %lld, delay = %lld\n", loops, delay);
        ret = measure(loops, delay);

        return EXIT_SUCCESS;
}
