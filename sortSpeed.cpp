/* g++ -O3 -o bgo sortSpeed.cpp -mrdrnd */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>


#define N ((1000000UL))
#define TYPE_LEN (sizeof(int))
#define DEF_HUGE_PAGE_SIZE ((0x200000))


void *ptr = NULL;
void *ptrd = NULL;
unsigned int *p = NULL;


long long unsigned start_ns;
struct timespec ts;

static inline long long unsigned time_ns(struct timespec* const ts) {
        if (clock_gettime(CLOCK_REALTIME, ts)) {
                exit(1);
        }
        return ((long long unsigned) ts->tv_sec) * 1000000000LLU
                        + (long long unsigned) ts->tv_nsec;
}


inline int cmp(const void *a, const void *b) {
	return (*(unsigned int*)a > *(unsigned int*)b) ? 1 : 0;
}


inline void allocMem(void **pptr, unsigned long long n_bytes, int hugepage_forced) {
	
        int hugepage_allocate = 1;
	int ret;
        if(hugepage_forced) {
                *pptr = mmap(NULL, ((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
                         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0) ;
                if(*pptr != MAP_FAILED) {
                        printf("HugePage allocated at pointer %p with size %llX\n", *pptr, n_bytes);
                        hugepage_allocate = 1;
                } else {
                        printf("HugePage allocated Failed: %d\n", *pptr);
                }
        }


        if(!hugepage_allocate) {
                ret = posix_memalign(pptr, sysconf(_SC_PAGESIZE), n_bytes);
                if (ret) {
                        fprintf(stderr,"None zero ret code %d\n", ret);
                        exit(EXIT_FAILURE);
                }
                printf("Normal page allocated at pointer %p with size %llX,\n\t" \
                        " * Please manually disable the Transparent Huge Page if needed.\n", *pptr, n_bytes);
	}
}


int main(int argc, char* argv[])
{

        unsigned long long *pos;
        unsigned long long i;
        unsigned long long start_ns;
        unsigned long long delta;
        struct timespec ts;

	int ret = 0;

	int hugepage_forced = 1;
	unsigned long long n_bytes = TYPE_LEN * N;


	allocMem(&ptr, n_bytes, hugepage_forced);

	unsigned long long steps = n_bytes / sizeof(unsigned long long);
	pos = (unsigned long long *)ptr;


	start_ns = time_ns(&ts);
	
	for(i = 0; i < steps; i++) {
        	__builtin_ia32_rdrand64_step(pos);
		pos++;
		//printf("%llX\n", *pos);
	}
	
        delta = time_ns(&ts) - start_ns;
	printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

	start_ns = time_ns(&ts);
	std::qsort(ptr, N, sizeof(unsigned int), cmp);
        delta = time_ns(&ts) - start_ns;
	printf("%p %016llX\n", ptr, *(unsigned long long *)ptr);
	printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

}
