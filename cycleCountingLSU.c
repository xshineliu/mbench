/* 
 * Counting cycles with the character of Load/INC/Store latency (L1 Cache) 
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_THREADS (1024)
#define LOOP (10 * 1000 * 1000)

#ifndef L1D_CL_BIT_SHIFT
#define L1D_CL_BIT_SHIFT (7)
#endif

#define L1D_CL_SIZE (1 << L1D_CL_BIT_SHIFT)


char data[MAX_THREADS + 1][L1D_CL_SIZE] __attribute__((aligned(L1D_CL_SIZE)));
int num_threads = 2;
int cpubind = 0;
volatile int timeout = 0; /* this data is shared by the thread(s) */
int delay_sec = 10;
int test_case = 0;

static inline long long unsigned time_ns(struct timespec* const ts) {
        if (clock_gettime(CLOCK_REALTIME, ts)) {
                exit(1);
        }
        return ((long long unsigned) ts->tv_sec) * 1000000000LLU
                + (long long unsigned) ts->tv_nsec;
}

static void print_help(const char *progname)
{
        printf("Usage: %s [OPTS]\n", progname);
        printf("-d      delay in second\n");
        printf("-n      number of threads\n");
        printf("-c      test case\n");
        printf("-b      Silly bind to each core\n");
        printf("-v      debug level\n");
}


void taskbind(int i) {
        if(!cpubind) {
                return;
        }
        cpu_set_t my_set;        /* Define your cpu_set bit mask. */
        CPU_ZERO(&my_set);       /* Initialize it all to 0, i.e. no CPUs selected. */
        CPU_SET(i, &my_set);     /* set the bit that represents core 7. */
        sched_setaffinity(0, sizeof(cpu_set_t), &my_set); /* Set affinity of tihs process to */
}


void *counting(void * param)
{
        int i, id;
        pid_t tid;
        unsigned long long *p = (unsigned long long *) param;
        long offset = (char *)param - (char *)data;
        id = (int) (offset >> L1D_CL_BIT_SHIFT);
        taskbind(id);
        switch (test_case) {
        case 1:
                while (!timeout){
                        __sync_fetch_and_add(p, 1L);
                }
                break;
        case 2:
                while (!timeout){
                        // x86ONLY
                        //__atomic_add_fetch(p, 1L, __ATOMIC_ACQUIRE|__ATOMIC_HLE_ACQUIRE);
                        __atomic_add_fetch(p, 1L, __ATOMIC_ACQUIRE);
                }
                break;
        case 3:
                while (!timeout){
                        __atomic_add_fetch(p, 1L, __ATOMIC_ACQUIRE);
                }
                break;
        default:
                while (!timeout){
                        (*p)++;
                        //(*p) += 10;
                }
        }

        p = (unsigned long long *) param;
        tid = syscall(SYS_gettid);
        printf("Thread id=%04d, tid=%06d, cnt=%llu\t(%X)\n", id, tid, *(unsigned long long *)p, p);
        pthread_exit(0);
}




int main(int argc, char *argv[])
{
        int i, j, opt;
        pthread_t tid[MAX_THREADS];              /* the thread identifiers      */
        pthread_attr_t attr; /* set of thread attributes */
        char *p = NULL;

        while ((opt = getopt(argc, argv, "n:d:c:b")) != -1) {
                switch (opt) {
                case 'n':
                        num_threads = strtoul(optarg, NULL, 0);
                        break;
                case 'd':
                        delay_sec = strtoul(optarg, NULL, 0);
                        break;
                case 'c':
                        test_case = strtoul(optarg, NULL, 0);
                        break;
                case 'b':
                        cpubind = 1;
                        break;
                default:
                        print_help(argv[0]);
                        return 0;
                }
        }

        if (num_threads < 1) {
                fprintf(stderr,"%d must be >= 1\n", num_threads);
                exit(EXIT_FAILURE);
        }

        if (num_threads > MAX_THREADS) {
                fprintf(stderr,"%d must be <= %d\n", num_threads, MAX_THREADS);
                exit(EXIT_FAILURE);
        }

        if (delay_sec < 1) {
                fprintf(stderr,"%d must be >= 1\n", delay_sec);
                exit(EXIT_FAILURE);
        }

        printf("The number of threads is %d, delay %d.\n", num_threads, delay_sec);

        /* get the default attributes */
        pthread_attr_init(&attr);

        p = (char *)data;
        /* create the threads */
        for (i = 0; i < num_threads; i++) {
                *(long long *)p = 0L;
                pthread_create(&(tid[i]), &attr, counting, (void *)p);
                p += L1D_CL_SIZE;
        }

        printf("Created %d threads with test case %d.\n", num_threads, test_case);


        sleep(delay_sec);
        timeout = 1;

        /* now wait for the threads to exit */
        for (i = 0; i < num_threads; i++) {
                pthread_join(tid[i], NULL);
        }


        printf("----------\n");
        return 0;
}
