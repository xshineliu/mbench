/* g++ -O3 -o bgo sortSpeed.cpp -mrdrnd */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
//#include <cstdint>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <x86intrin.h>

//#define N ((1000000UL))
#define N ((1000000UL))
#define TYPE_LEN (sizeof(int))
#define DEF_HUGE_PAGE_SIZE ((0x200000))


void *ptr = NULL;
void *ptrd = NULL;
unsigned int *p = NULL;

#ifdef DEBUG
static int depth = 0;
static int depthMax = 0;
static int numSplit = 0;
#endif


long long unsigned start_ns;
struct timespec ts;

/*
 * newRBound --->  <--- newLBound
 */

void scalar_partition_epi32(unsigned int ** newRBound, unsigned int** newLBound, const unsigned int pivot) {

#ifdef DEBUG
    numSplit++;
#endif

    int tmp = 0;

    while (*newRBound <= *newLBound) {

        //__builtin_prefetch(*newLBound, 0, 3);
        while (**newRBound < pivot) {
            (*newRBound)++;
        }

        while (**newLBound > pivot) {
            (*newLBound)--;
        }

        if (*newRBound <= *newLBound) {
            const unsigned int t = **newRBound;
            **newRBound     = **newLBound;
            **newLBound     = t;

            (*newRBound)++;
            (*newLBound)--;
        }
    }
}


/* 
 *  Left ... newRBound || newLBound ... Right
 *  lBound ... newRBound || newLBound ... rBound
 */

void qs_recsv(unsigned int* array, unsigned long long nElem) {

    unsigned int *lBound = array;
    unsigned int *rBound = array + nElem - 1;
    unsigned int *newRBound = lBound;
    unsigned int *newLBound = rBound;

    //const unsigned int pivot = array[nElem / 2];
    const unsigned int pivot = array[0];

    scalar_partition_epi32(&newRBound, &newLBound, pivot);

    if (lBound < newLBound) {
#ifdef DEBUG
        depth++;
	depthMax = (depth > depthMax) ? depth : depthMax;
#endif
        qs_recsv(lBound, newLBound - lBound);
    }


    if (newRBound < rBound) {
#ifdef DEBUG
        depth++;
	depthMax = (depth > depthMax) ? depth : depthMax;
#endif
        qs_recsv(newRBound, rBound - newRBound);
    }

#ifdef DEBUG
    depth--;
#endif
}



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
	
        int hugepage_allocate = 0;
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

void measure() {
	unsigned long long delta = 0;
	unsigned long long mid = (N + 1) / 2;
	unsigned int *arrA = (unsigned int *) ptr;
	unsigned int *arrB = (unsigned int *) ptrd;
	unsigned int midVal = arrA[mid];

	unsigned long long  i = 0;
	unsigned long long  j = 0;
	unsigned long long  k = (N - 1);

	start_ns = time_ns(&ts);
	for (i = 0 ; i < N; i++) {
		if (arrA[i] < mid) {
			arrB[j++] = arrA[i];
		} else {
			arrB[k--] = arrA[i];
		}
	}
        delta = time_ns(&ts) - start_ns;

	printf("*** COPY %p %016llX, %ld %ld %ld, %ld\n", ptrd, *(unsigned long long *)ptrd, i, j, k, delta);
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
	allocMem(&ptrd, n_bytes, hugepage_forced);

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
	//std::qsort(ptr, N, sizeof(unsigned int), cmp);
	qs_recsv((unsigned int *)ptr,  N);
        delta = time_ns(&ts) - start_ns;
	printf("%p %016llX\n", ptr, *(unsigned long long *)ptr);
	printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);


/*
	measure();


	start_ns = time_ns(&ts);
	std::qsort(ptrd, N, sizeof(unsigned int), cmp);
        delta = time_ns(&ts) - start_ns;
	printf("%p %016llX\n", ptrd, *(unsigned long long *)ptrd);
	pos = (unsigned long long *)ptrd + steps;
	printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);
*/

#ifdef DEBUG
	printf("Max Stack Depth %d, number of splits %d\n", depthMax, numSplit);
#endif
}
