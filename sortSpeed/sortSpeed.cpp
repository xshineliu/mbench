/* icpc -O3 -static -o fff ./sortTesting.cpp ./qsort_avx2_int32.o /opt/intel/compilers_and_libraries_2018.1.163/linux/ipp/lib/intel64/libippcore.a /opt/intel/compilers_and_libraries_2018.1.163/linux/ipp/lib/intel64/libipps.a -march=core2 -mavx2 -mrdrnd -lippcore -lipps */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
//#include <cstdint>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <x86intrin.h>

#ifdef __ICC
#include <ipp.h>
#include <ipps.h>
#endif

#define VAL_TYPE unsigned int

#ifndef N
#define N ((1000000UL))
#endif

#define MIN_N_ELEM 16
#define TYPE_LEN (sizeof(VAL_TYPE))

#ifndef DEF_HUGE_PAGE_SIZE
#define DEF_HUGE_PAGE_SIZE ((0x200000))
#endif

void *ptr = NULL;
void *ptrd = NULL;
VAL_TYPE *p = NULL;

#ifdef DEBUG
static int depth = 0;
static int depthMax = 0;
static int numSplit = 0;
#endif

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
	return (*(VAL_TYPE*) a > *(VAL_TYPE*) b) ? 1 : 0;
}

void allocMem(void **pptr, size_t n_bytes, int hugepage_forced) {

	int hugepage_allocate = 0;
	int ret;
	if (hugepage_forced) {
		*pptr = mmap(NULL,
				((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (*pptr != MAP_FAILED) {
			printf("HugePage allocated at pointer %p with size %llX\n", *pptr,
					n_bytes);
			hugepage_allocate = 1;
		} else {
			printf("HugePage allocated Failed: %d\n", *pptr);
		}
	}

	if (!hugepage_allocate) {
		ret = posix_memalign(pptr, sysconf(_SC_PAGESIZE), n_bytes);
		if (ret) {
			fprintf(stderr, "None zero ret code %d\n", ret);
			exit (EXIT_FAILURE);
		}
		printf(
				"Normal page allocated at pointer %p with size %llX,\n\t"
						" * Please manually disable the Transparent Huge Page if needed.\n",
				*pptr, n_bytes);
	}
}

/*
 * newRBound --->  <--- newLBound
 */

void scalar_partition_epi32(VAL_TYPE** newRBound, VAL_TYPE** newLBound,
		const VAL_TYPE pivot) {

#ifdef DEBUG
	numSplit++;
#endif

	int tmp = 0;

	while (*newRBound <= *newLBound) {

		//__builtin_prefetch(*newLBound, 0, 0);
		while (**newRBound < pivot) {
			(*newRBound)++;
		}

		while (**newLBound > pivot) {
			(*newLBound)--;
		}

		if (*newRBound <= *newLBound) {
			const VAL_TYPE t = **newRBound;
			**newRBound = **newLBound;
			**newLBound = t;

			(*newRBound)++;
			(*newLBound)--;
		}
	}
}

/*
 *  lBound ... newRBound ---> <--- newLBound ... rBound
 */

void qs_recsv(VAL_TYPE* array, size_t nElems) {

	VAL_TYPE *lBound = array;
	VAL_TYPE *rBound = array + nElems - 1;
	VAL_TYPE *newRBound = lBound;
	VAL_TYPE *newLBound = rBound;

	//const VAL_TYPE pivot = array[nElem / 2];
	const VAL_TYPE pivot = array[0];

	scalar_partition_epi32(&newRBound, &newLBound, pivot);

	if (lBound < newLBound) {
#ifdef DEBUG
		depth++;
		depthMax = (depth > depthMax) ? depth : depthMax;
#endif
		qs_recsv(lBound, newLBound + 1 - lBound);
	}

	if (newRBound < rBound) {
#ifdef DEBUG
		depth++;
		depthMax = (depth > depthMax) ? depth : depthMax;
#endif
		qs_recsv(newRBound, rBound + 1 - newRBound);
	}

#ifdef DEBUG
	depth--;
#endif
}

extern "C" {
int qsort_AVX2(void*, void*, unsigned long long n);
}

static void avx2_remain_sorts(int *d, unsigned long long n) {
	if (n <= 1)
		return;
	unsigned long long i, j;
	for (i = 1; i < n; i++) {
		int tmp = d[i];
		for (j = i; j >= 1 && tmp < d[j - 1]; j--)
			d[j] = d[j - 1];
		d[j] = tmp;
	}
}

void my_qsort_AVX2(int* unsorted_array, int* tmp_array, size_t n) {
	size_t new_n;
	while (n > 32) {
		new_n = qsort_AVX2(unsorted_array, tmp_array, n);
		n = n - new_n - 1;
		my_qsort_AVX2(&unsorted_array[n + 1], tmp_array, new_n);
	}
	avx2_remain_sorts(unsorted_array, n);
}




/*
 * small --- equal --- large filter to dst
 */
void measurePovit1(VAL_TYPE *arr, VAL_TYPE *dst, size_t nLen) {
	unsigned long long delta = 0;

	VAL_TYPE *arrSrc = arr;
	VAL_TYPE *arrDstL = dst;
	VAL_TYPE *arrDstR = dst + nLen - 1;
	VAL_TYPE midVal = arr[nLen / 2];

	start_ns = time_ns(&ts);
	while (arrSrc < arr + nLen) {
		if (*arrSrc < midVal) {
			*arrDstL = *arrSrc;
			arrDstL++;
		} else if (*arrSrc > midVal) {
			*arrDstR = *arrSrc;
			arrDstR--;
		}
		arrSrc++;
	}
	delta = time_ns(&ts) - start_ns;

	printf("*** COPY %p %ld %ld %ld\n", dst, arrDstL - dst, arrDstR - dst,
			delta);
}

void measurePovit2(VAL_TYPE *arr, VAL_TYPE *dst, size_t nLen) {

	unsigned long long delta = 0;

	VAL_TYPE *arrSrcL = arr;
	VAL_TYPE *arrSrcR = arr + nLen - 1;
	VAL_TYPE *arrDstL = dst;
	VAL_TYPE *arrDstR = dst + nLen - 1;
	VAL_TYPE midVal = arr[nLen / 2];

	start_ns = time_ns(&ts);

	while (arrSrcL <= arrSrcR) {

		//__builtin_prefetch(*newLBound, 0, 0);
		while (*arrSrcL < midVal) {
			*arrDstL = *arrSrcL;
			arrDstL++;
			arrSrcL++;
		}

		while (*arrSrcR > midVal) {
			*arrDstR = *arrSrcR;
			arrDstR--;
			arrSrcR--;
		}

		if (arrSrcL <= arrSrcR) {
			*arrDstL = *arrSrcR;
			arrDstL++;
			arrSrcR--;
			*arrDstR = *arrSrcL;
			arrDstR--;
			arrSrcL++;
		}

	}

	delta = time_ns(&ts) - start_ns;

	printf("*** COPY %p %ld %ld %ld\n", dst, arrDstL - dst, arrDstR - dst,
			delta);
}

int main(int argc, char* argv[]) {

	unsigned long long *pos;
	unsigned long long i;
	unsigned long long start_ns;
	unsigned long long delta;
	struct timespec ts;
	size_t nElems = N;

	int test_case = -1;
	int hugepage_forced = 0;

	int opt;
	int ret = 0;
	unsigned long long n_bytes = TYPE_LEN * nElems;

	while ((opt = getopt(argc, argv, "n:lc:")) != -1) {
		switch (opt) {
		case 'n':
			nElems = strtoul(optarg, NULL, 0);
			if (nElems < MIN_N_ELEM) {
				nElems = MIN_N_ELEM;
			}
			n_bytes = TYPE_LEN * nElems;
			break;
		case 'l':
			hugepage_forced = 1;
			break;
		case 'c':
			test_case = strtoul(optarg, NULL, 0);
			break;
		default:
			//print_help(argv[0]);
			exit (EXIT_FAILURE);
		}
	}

	allocMem(&ptr, n_bytes, hugepage_forced);
	allocMem(&ptrd, n_bytes * 11 / 10, hugepage_forced);

	unsigned long long steps = n_bytes / sizeof(unsigned long long);

	if (test_case == 0) {
		pos = (unsigned long long *) ptr;
		start_ns = time_ns(&ts);
		for (i = 0; i < steps; i++) {
			__builtin_ia32_rdrand64_step(pos);
			pos++;
		}
		delta = time_ns(&ts) - start_ns;
		printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

		start_ns = time_ns(&ts);
		// dummy
		delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld\n", 0, delta, steps);
	}

	if (test_case < 0 || test_case == 1) {
		pos = (unsigned long long *) ptr;
		start_ns = time_ns(&ts);
		for (i = 0; i < steps; i++) {
			__builtin_ia32_rdrand64_step(pos);
			pos++;
		}
		delta = time_ns(&ts) - start_ns;
		printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

		start_ns = time_ns(&ts);
		std::qsort(ptr, nElems, sizeof(VAL_TYPE), cmp);
		delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld\n", 1, delta, steps);
	}



	if (test_case < 0 || test_case == 2) {
		pos = (unsigned long long *) ptr;
		start_ns = time_ns(&ts);
		for (i = 0; i < steps; i++) {
			__builtin_ia32_rdrand64_step(pos);
			pos++;
		}
		delta = time_ns(&ts) - start_ns;
		printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

		start_ns = time_ns(&ts);
		qs_recsv((VAL_TYPE *) ptr, nElems);
		delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld\n", 2, delta, steps);
#ifdef DEBUG
		printf("Max Stack Depth %d, number of splits %d\n", depthMax, numSplit);
#endif
	}



	if (test_case < 0 || test_case == 3) {
		pos = (unsigned long long *) ptr;
		start_ns = time_ns(&ts);
		for (i = 0; i < steps; i++) {
			__builtin_ia32_rdrand64_step(pos);
			pos++;
		}
		delta = time_ns(&ts) - start_ns;
		printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

		start_ns = time_ns(&ts);
		my_qsort_AVX2((int *) ptr, (int *) ptrd, nElems);
		delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld\n", 3, delta, steps);
	}


#ifdef __ICC
	if(test_case < 0 || test_case == 4){
        pos = (unsigned long long *)ptr;
        start_ns = time_ns(&ts);
        for(i = 0; i < steps; i++) {
                __builtin_ia32_rdrand64_step(pos);
                pos++;
        }
        delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %ld %ld\n", pos - 1, *(pos - 1), delta, steps);

        ippInit();
        int ippSizeVal = 0;
        ippsSortRadixGetBufferSize(N, ipp32u, &ippSizeVal);
        start_ns = time_ns(&ts);
        /* https://software.intel.com/en-us/node/722934 */
        ret = ippsSortRadixAscend_32u_I((Ipp32u*)ptr, nElems, (Ipp8u*)ptrd);
        delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld %ld %ld\n", 4, delta, steps, ippSizeVal, ret);
	}
#endif

	//measurePovit2((VAL_TYPE*)ptr, (VAL_TYPE*)ptrd, nElems);

}

