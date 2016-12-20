/*
 ============================================================================
 Author      : Shine Liu
 Version     : 0.02 / fix for 32 bit / 2016-12-20
 Date        : 2016-12-20
 Copyright   : TODO
 Description : Memory Performance measure_rnd_readment
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

#define MAX_THREADS     (256)
#define DEFAULT_LOOP    (1000)
#define N               (64 * 1024 * 1024)

#define RD_ONE  p = (char **)*p;
#define RD_FIVE RD_ONE RD_ONE RD_ONE RD_ONE RD_ONE
#define RD_TEN  RD_FIVE RD_FIVE
#define RD_FIFTY        RD_TEN RD_TEN RD_TEN RD_TEN RD_TEN
#define RD_HUNDRED      RD_FIFTY RD_FIFTY


//#define WR_ONE        q[1] = 0x1LLU; q = (unsigned long long *)(void *)(*q);
#define WR_ONE  q[1] = 0x1LLU; q = (unsigned long long *)(uintptr_t)(*q);
#define WR_FIVE WR_ONE WR_ONE WR_ONE WR_ONE WR_ONE
#define WR_TEN  WR_FIVE WR_FIVE
#define WR_FIFTY        WR_TEN WR_TEN WR_TEN WR_TEN WR_TEN
#define WR_HUNDRED      WR_FIFTY WR_FIFTY

#ifndef DEF_HUGE_PAGE_SIZE
#define DEF_HUGE_PAGE_SIZE (0x200000LLU)
#endif


//long long a[N] __attribute__((aligned(0x1000)));
//long long * ptr[MAX_THREADS] __attribute__((aligned(0x1000))) ;

unsigned long long *ptr = NULL;
unsigned long long *ptrs[MAX_THREADS] = {NULL, };
unsigned long long *rnd_ptrs[MAX_THREADS] = {NULL, };
unsigned long long  debug_ret[MAX_THREADS][4];

//#pragma omp threadprivate(ptr)


int num_threads = 1;
int omp_num_threads = 1;
int mem_size_per_thread_in_kb = 16; // 16M
int test_case = 0;
int cpubind = 0;
int stride = 256;
int hugepage_allocate = 0;
int hugepage_forced = 0;
double rand_ratio = 0.95;
int mega = 1; // 1 fir 1K,and 1024 for 1M


// global calculated out
unsigned long long n_bytes;


int default_loop = DEFAULT_LOOP;

static inline unsigned long long time_ns(struct timespec* const ts) {
        if (clock_gettime(CLOCK_REALTIME, ts)) {
                exit(1);
        }
        return ((unsigned long long) ts->tv_sec) * 1000000000LLU
                        + (unsigned long long) ts->tv_nsec;
}


double parallel_memset(char* addr, unsigned long long size) {

        unsigned long long my_bytes;
        unsigned long long delta;
        unsigned long long start_ns;
        struct timespec ts;
        int omp_num_threads = 0, i = 0;
        char *pos = NULL;


#pragma omp parallel private(pos, i)
        {
#pragma omp single
                {
                        omp_num_threads = omp_get_num_threads();
                        my_bytes = size / omp_num_threads;
                }
#if 0
// omp single implies an implicit barrier here, while the omp master doesn't
                i = omp_get_thread_num();
                pos = addr + i * my_bytes; 
                rnd_ptrs[i] =  (unsigned long long *)pos;
#endif
        }

        start_ns = time_ns(&ts);
#pragma omp parallel private(pos, i)
        {
                i = omp_get_thread_num();
                pos = addr + i * my_bytes; 
                memset((void *)pos, 0x0, my_bytes);
        }

        delta = time_ns(&ts) - start_ns;
        return delta;
}



double measure_rnd_read(char **paddr, unsigned int itr1, unsigned long long itr2){
        unsigned long long delta = 0;
        unsigned long long i = 0;
        register char **p = (char **)(*paddr);
        unsigned long long start_ns;
        struct timespec ts;

        start_ns = time_ns(&ts);
        while (itr1-- > 0) {
                for(i = 0; i < itr2; i++) {
                        RD_HUNDRED;
                }
                //printf("%d\n", iterations);
        }

        delta = time_ns(&ts) - start_ns;
        /* avoid compiler optimization */
        *paddr = *p;
        //printf("%p, %p\n", p, *p);
        //printf("%llu, %.3f\n", delta, (double)delta / (double) itr2 / (double)itr1 / 100LLU);
        return delta;
}


double measure_rnd_fill(unsigned long long **paddr, unsigned int itr1, unsigned long long itr2){
        unsigned long long delta = 0;
        unsigned long long i = 0;
        register unsigned long long *q = *paddr;
        unsigned long long start_ns;
        struct timespec ts;

        start_ns = time_ns(&ts);
        while (itr1-- > 0) {
                for(i = 0; i < itr2; i++) {
                        WR_HUNDRED;
                        //q = (unsigned long long *)(uintptr_t)(*q);
                }
                //printf("%d\n", iterations);
        }

        delta = time_ns(&ts) - start_ns;
        /* avoid compiler optimization */
        *paddr = q;
        //printf("%p, %p\n", p, *p);
        //printf("%llu, %.3f\n", delta, (double)delta / (double) itr2 / (double)itr1 / 100LLU);
        return delta;
}

double setup_rnd(unsigned long long *addr, unsigned long long size, int seg_size, int myidx) {

        unsigned long long delta = 0;

        unsigned long long *tmp, *pos;
        unsigned long long n_done = 0;
        unsigned long long n_conflict = 0;
        unsigned long long rnd;

        unsigned long long start_ns;
        struct timespec ts;

        unsigned long long n_segs = size / seg_size;
        // check n_segs * seg_size == size !!!
        pos = addr;
        //printf("%d - %p: %lld * %d\n", myidx, addr, n_segs, seg_size);


#ifndef HAS_RND_INS
        size_t nread = 0;
        FILE * fp = fopen("/dev/urandom", "r");
        if(!fp) {
                fprintf(stderr, "Error opening /dev/urandom\n");
                exit(-1);
        }
#endif

        start_ns = time_ns(&ts);
        do {
#ifndef HAS_RND_INS
                nread = fread(&rnd, sizeof(unsigned long long), 1, fp);
                if(nread != 1) {
                        fprintf(stderr, "Error reading /dev/urandom, return %d\n", nread);
                        exit(-1);
                }
#else
                __builtin_ia32_rdrand64_step(&rnd);
#endif
                tmp =  (unsigned long long *)((void *)addr + (rnd % n_segs) * seg_size);
                if(*tmp) {
                        // already has data, conflict, try next?
                        if(size > (0x01L << 28) && n_done > rand_ratio * n_segs) {
                                break;
                        //} else if {
                                // confilict handler
                        //      ;
                        } else {
                                n_conflict++;
                                //if(n_conflict > 12LL * n_segs)
                                //      printf("%x\n", n_conflict);
                                continue;
                        }
                }
                // adaptive for 32bit mode
                *pos = (uintptr_t)tmp;
                pos = tmp;
                n_done++;
        } while(n_done < n_segs);
        // make a ring
        *pos = (uintptr_t)addr;

        delta = time_ns(&ts) - start_ns;

        debug_ret[myidx][0] = n_done;
        debug_ret[myidx][1] = n_conflict;
        debug_ret[myidx][2] = (uintptr_t) pos;
        debug_ret[myidx][3] =  *pos;

#ifndef HAS_RND_INS
        fclose(fp);
#endif
        return delta;
}



double parallel_test() {
        unsigned long long delta = 0;
        unsigned long long my_bytes = 0;
        unsigned long long n_segs = 0;
        unsigned long long *pos = NULL;
        int idx, omp_num_threads, i;

        unsigned long long n_done = 0;
        unsigned long long n_conflict = 0;

        unsigned long long start_ns;
        struct timespec ts;

        printf("\n=== %s ===\n", __FUNCTION__);

        start_ns = time_ns(&ts);
#pragma omp parallel private(idx)
        {
                omp_num_threads = omp_get_num_threads();
                my_bytes = n_bytes / omp_num_threads;

                idx = omp_get_thread_num();
                //printf("%d, %d, %p, %llx\n", idx, omp_num_threads, ptrs[idx], my_bytes);
                setup_rnd(ptrs[idx], my_bytes, stride, idx); 
        }


        delta = time_ns(&ts) - start_ns;
        for(i = 0; i < omp_num_threads; i++) {
                n_done += debug_ret[i][0];
                n_conflict += debug_ret[i][1];
        }

        n_segs = n_bytes / stride;
        printf("%.3f ms with each %.03f ns; %lld + %lld (ratio %.3f), target is %lld, archieved %.2f%%.\n", 
                (double)delta / 1000000.f, (double)delta / (n_conflict + n_done), 
                n_done, n_conflict, (double)(n_done, n_conflict)/(double)n_done, 
                n_segs, (double)n_done / (double)n_segs * 100.f);


        /* TEST1 */

        pos = ptr;
        delta = measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread read: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        start_ns = time_ns(&ts);
#pragma omp parallel private(idx, pos)
        {
                idx = omp_get_thread_num();
                pos = ptrs[idx];
                //printf("### %d: %p, %p\n", idx, &pos, pos);
                measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        }
        delta = time_ns(&ts) - start_ns;
        printf("%s with %d threads read: %.3f ms, %.3f ns each.\n", __FUNCTION__, omp_num_threads,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);



        /* TEST2 */

        pos = ptr;
        delta = measure_rnd_fill(&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread fill: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        start_ns = time_ns(&ts);
#pragma omp parallel private(idx, pos)
        {
                idx = omp_get_thread_num();
                pos = ptrs[idx];
                //printf("### %d: %p, %p\n", idx, &pos, pos);
                measure_rnd_fill(&pos, default_loop, 1000LLU * 10LLU);
        }
        delta = time_ns(&ts) - start_ns;
        printf("%s with %d threads fill: %.3f ms, %.3f ns each.\n", __FUNCTION__, omp_num_threads,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


}

void simple_test() {

        int i, opt, ret = 0;
        unsigned long long n_segs;

        unsigned long long delta;
        unsigned long long *pos = NULL;
        unsigned long long n_done;
        unsigned long long n_conflict;

        unsigned long long start_ns;
        struct timespec ts;


        printf("\n=== %s ===\n", __FUNCTION__);

        n_segs = n_bytes / stride;
        delta = setup_rnd(ptr, n_bytes, stride, 0);
        n_done = debug_ret[0][0];
        n_conflict = debug_ret[0][1];

        printf("%016lX : %016lx (base %016lX: %016lX).\n", debug_ret[0][2], debug_ret[0][3], ptr, *ptr);
        printf("%.3f ms with each %.03f ns; %lld + %lld (ratio %.3f), target is %lld, archieved %.2f%%.\n", 
                (double)delta / 1000000.f, (double)delta / (n_conflict + n_done), 
                n_done, n_conflict, (double)(n_done, n_conflict)/(double)n_done, 
                n_segs, (double)n_done / (double)n_segs * 100.f);

        /* TEST1 */

        pos = ptr;
        delta = measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread read: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        pos = ptr;
        start_ns = time_ns(&ts);
#pragma omp parallel firstprivate(pos) num_threads(num_threads)
        {
                measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        }
        delta = time_ns(&ts) - start_ns;
        printf("%s with %d threads read: %.3f ms, %.3f ns each.\n", __FUNCTION__, num_threads,
                (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        /* TEST 2 */

        pos = ptr;
        delta = measure_rnd_fill(&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread fill: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        pos = ptr;
        start_ns = time_ns(&ts);
#pragma omp parallel firstprivate(pos) num_threads(num_threads)
        {
                measure_rnd_fill(&pos, default_loop, 1000LLU * 10LLU);
        }
        delta = time_ns(&ts) - start_ns;
        printf("%s with %d threads fill: %.3f ms, %.3f ns each.\n", __FUNCTION__, num_threads,
                (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


}


void modify_read() {

        int n_threads = 2;
        unsigned long long  n_segs;

        unsigned long long delta;
        unsigned long long *pos = NULL;
        unsigned long long n_done;
        unsigned long long n_conflict;

        unsigned long long start_ns;
        struct timespec ts;

        printf("\n=== %s ===\n", __FUNCTION__);

        n_segs = n_bytes / stride;
        delta = setup_rnd(ptr, n_bytes, stride, 0);
        n_done = debug_ret[0][0];
        n_conflict = debug_ret[0][1];

        printf("%016lX : %016lx (base %016lX: %016lX).\n", debug_ret[0][2], debug_ret[0][3], ptr, *ptr);
        printf("%.3f ms with each %.03f ns; %lld + %lld (ratio %.3f), target is %lld, archieved %.2f%%.\n", 
                (double)delta / 1000000.f, (double)delta / (n_conflict + n_done), 
                n_done, n_conflict, (double)(n_done, n_conflict)/(double)n_done, 
                n_segs, (double)n_done / (double)n_segs * 100.f);


        /* BASE */


        pos = ptr;
        start_ns = time_ns(&ts);
#pragma omp parallel firstprivate(pos) num_threads(n_threads)
        {
                measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        }
        delta = time_ns(&ts) - start_ns;
        printf("%s with %d threads read: %.3f ms, %.3f ns each.\n", __FUNCTION__, n_threads,
                (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);


        pos = ptr;
        delta = measure_rnd_read((char **)&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread read: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);

        pos = ptr;
        delta = measure_rnd_fill(&pos, default_loop, 1000LLU * 10LLU);
        printf("%s single thread fill: %.3f ms, %.3f ns each.\n", __FUNCTION__,
                 (double) delta / 1000000.f, (double)delta / (double) ( 1000LLU * 10LLU) / (double)default_loop / 100LLU);
}


void print_help(char *s) {
        printf("args error\n"  \
                "\t-n num_threads\n" \
                "\t-s mem_size_per_thread_in_kb in KB (unless with -m)\n" \
                "\t-m (mega unit true)\n" \
                "\t-l (hugepage_forced true)\n" \
                "\t-d stride\n" \
                "\t-c test_case\n" \
                "\t-b (cpubind true)\n" \
                "\t-q random ratio: valid value from 80-100\n" \
        );
}

int main(int argc, char *argv[])
{
        int i, opt, ret = 0;
        int no_default_size = 0;
        unsigned long long  n_segs;
        unsigned long long delta, j;
        unsigned long long *pos = NULL;
        unsigned long long start_ns;
        struct timespec ts;

        while ((opt = getopt(argc, argv, "n:s:mld:c:b:q:")) != -1) {
                switch (opt) {
                case 'n':
                        num_threads = strtoul(optarg, NULL, 0);
                        break;
                case 's':
                        mem_size_per_thread_in_kb = strtoul(optarg, NULL, 0);
                        no_default_size = 1;
                        break;
                case 'm':
                        mega = 1024L;
                        default_loop = default_loop / 100;
                        break;
                case 'l':
                        hugepage_forced = 1;
                        break;
                case 'd':
                        stride = strtoul(optarg, NULL, 0);
                        break;
                case 'c':
                        test_case = strtoul(optarg, NULL, 0);
                        break;
                case 'b':
                        cpubind = 1;
                        break;
                case 'q':
                        rand_ratio = (double)(strtoul(optarg, NULL, 0)) * 0.01f;
                        break;
                default:
                        print_help(argv[0]);
                        exit(EXIT_FAILURE);
                }
        }

        if(no_default_size && mega > 1) {
                mem_size_per_thread_in_kb *= 1024L;
        }

        if (mem_size_per_thread_in_kb < 4) {
                fprintf(stderr,"%d must be >= 4\n", mem_size_per_thread_in_kb);
                exit(EXIT_FAILURE);
        }

        if (num_threads > MAX_THREADS || num_threads < 1) {
                fprintf(stderr,"%d must be between %d to %d\n", num_threads, 1, MAX_THREADS);
                exit(EXIT_FAILURE);
        }

        if(rand_ratio < 0.80 || rand_ratio > 1.00) {
                fprintf(stderr, "rand_ratio %f must between 80 to 100.\n", rand_ratio);
                exit(EXIT_FAILURE);
        }

#pragma omp parallel
        {
                omp_num_threads = omp_get_num_threads();
        }

        if(omp_num_threads > MAX_THREADS) {
                printf("%d exceeding maximum %d threads supported\n", omp_num_threads, MAX_THREADS);
                exit(EXIT_FAILURE);
        }

        n_bytes = mem_size_per_thread_in_kb * 1024L * omp_num_threads;
        n_segs = n_bytes / stride;
        //printf("*** DEBUG: n_bytes=%lld, n_segs=%lld, stride=%d\n", n_bytes, n_segs, stride);

        if(hugepage_forced) {
                ptr = mmap(NULL, ((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
                         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0) ;
                if(ptr != MAP_FAILED) {
                        printf("HugePage allocated at pointer %p\n", ptr);
                        hugepage_allocate = 1;
                } else {
                        printf("HugePage allocated Failed: %d\n", ptr);
                }
        }


        if(!hugepage_allocate) {
                ret = posix_memalign((void **)&ptr, sysconf(_SC_PAGESIZE), n_bytes);
                if (ret) {
                        fprintf(stderr,"None zero ret code %d\n", ret);
                        exit(EXIT_FAILURE);
                }
                printf("Normal page allocated at pointer %p,\n\t" \
                        " * Please manually disable the Transparent Huge Page if needed.\n", ptr);
        }

        // under 32bit mode, explicitly convert from 64bit int to 32 bit int is a MUST !!!
        printf("Using size: %d KiB (%f MiB) with stride %d (%lld segs)\n", (int) (n_bytes / 1024UL),
                 ((double) (n_bytes / 1024UL)) / (double) 1024.0f, stride, n_segs);


        start_ns = time_ns(&ts);
#pragma omp parallel private(i)
        {
                i = omp_get_thread_num();
                ptrs[i] =  (unsigned long long *)((void *)ptr + i * mem_size_per_thread_in_kb * 1024L);

#pragma omp critical
                printf("#### %04d/%04d : %016lX\n", i, omp_num_threads, ptrs[i]);

                memset((void *)ptrs[i], 0x0, mem_size_per_thread_in_kb * 1024L);
        }

        delta = time_ns(&ts) - start_ns;
        printf("%.06f ms:\t%.02f GB/s [Memset with page fault BW]\n", (double)delta / 1000000.0f,  mem_size_per_thread_in_kb * 1024L * omp_num_threads / (double)delta );



        simple_test();

        delta = parallel_memset((char *)ptr, n_bytes);
        printf("%.06f ms:\t%.02f GB/s [Memset without page fault BW]\n", (double)delta / 1000000.0f,  mem_size_per_thread_in_kb * 1024L * omp_num_threads / (double)delta );

        parallel_test();

        delta = parallel_memset((char *)ptr, n_bytes);
        printf("%.06f ms:\t%.02f GB/s [Memset without page fault BW]\n", (double)delta / 1000000.0f,  mem_size_per_thread_in_kb * 1024L * omp_num_threads / (double)delta );


        modify_read();


        return EXIT_SUCCESS;
}
