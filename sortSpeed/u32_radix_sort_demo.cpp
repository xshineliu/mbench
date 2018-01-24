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

#ifdef __ICC
#include <ipp.h>
#include <ipps.h>
#endif

#define EXTRA_BYTES 0x5060

#define VAL_TYPE unsigned int

#ifndef N
#define N ((1000))
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


void hexdump(void *mem, size_t len)
{
	const char * sep = "    ";
	char* loc = (char *)mem;

	int n_padding = (0x10 - (int)((unsigned long)mem & 0xFUL)) & 0xF;
	//printf("%p, %d", mem, n_padding);
	int n_line;
	int n_left = 0;
	int pos = 0;


	if(n_padding) {
		n_left = n_padding;
		printf("%p\t", (unsigned long)mem - n_padding);
		while(n_left) {
			printf("   ");
			n_left--;
			pos++;
			if((pos % 4) == 0) {
				printf("%s", sep);
			}
		}

		n_left = 0x10 - n_padding;
		while(n_left) {
			printf("%X ", *loc & 0xFF);
			loc++;
			n_left--;
			len--;

			pos++;
			if((pos % 4) == 0) {
				printf("%s", sep);
			}

		}
		printf("\n");
	}


	while(len > 0xF) {
		printf("%p\t", loc);
		printf("%02X %02X %02X %02X%s", loc[0] & 0xFF, loc[1] & 0xFF, loc[2] & 0xFF, loc[3] & 0xFF, sep);
		loc += 0x4;
		printf("%02X %02X %02X %02X%s", loc[0] & 0xFF, loc[1] & 0xFF, loc[2] & 0xFF, loc[3] & 0xFF, sep);
		loc += 0x4;
		printf("%02X %02X %02X %02X%s", loc[0] & 0xFF, loc[1] & 0xFF, loc[2] & 0xFF, loc[3] & 0xFF, sep);
		loc += 0x4;
		printf("%02X %02X %02X %02X%s", loc[0] & 0xFF, loc[1] & 0xFF, loc[2] & 0xFF, loc[3] & 0xFF, sep);
		loc += 0x4;
		len -= 0x10;
		printf("\n", loc);
	}


	if(len > 0) {
		printf("%p\t", loc);
		pos = 0;
		while(len) {
			printf("%X ", *loc & 0xFF);
			loc++;
			len--;

			pos++;
			if((pos % 4) == 0) {
				printf("%s", sep);
			}
		}
	}

}


/* only handle n < INT_MAX */

void radix_sort(VAL_TYPE* const loc, void* const buf, size_t const n) {
	int i;
	int idx;
	int *seqTbl;

	// TODO, n >  INT_MAX not support

	seqTbl = (int *)(buf);
	seqTbl[idx] = 0;
	for(i = 0; i < n; i++) {
		/* lower 11 bits */
		idx = loc[i] & 0x7FF;
		//printf("%d, %X, %08X\n", idx, idx, loc[i]);
		seqTbl[idx + 1]++;
	}

	seqTbl = (int *)((char *)buf + 0x2020);
	seqTbl[idx] = 0;
	for(i = 0; i < n; i++) {
		/* middle 11 bits */
		idx = (loc[i] >> 11) & 0x7FF;
		//printf("%d, %X, %08X\n", idx, idx, loc[i]);
		seqTbl[idx + 1]++;
	}

	seqTbl = (int *)((char *)buf + + 0x4040);
	seqTbl[idx] = 0;
	for(i = 0; i < n; i++) {
		/* upper 10 bits */
		idx = (loc[i] >> 22) & 0x3FF;
		//printf("%d, %X, %08X\n", idx, idx, loc[i]);
		seqTbl[idx + 1]++;
	}



	//hexdump((char *)buf, 0x5060);


	seqTbl = (int *)((char *)buf);
	//printf("%p %08X %08X\n", seqTbl, seqTbl[0], seqTbl[1]);
	for(idx = 1; idx < 2048; idx++) {
		int val_a = seqTbl[idx - 1];
		int val_b = seqTbl[idx];
		seqTbl[idx] = val_a + val_b;
		//printf("%p %d: %d %d %d\n", seqTbl, idx - 1, val_a, val_b, seqTbl[idx]);
	}

	seqTbl = (int *)((char *)buf + 0x2020);
	//printf("%p %08X %08X\n", seqTbl, seqTbl[0], seqTbl[1]);
	for(idx = 1; idx < 2048; idx++) {
		int val_a = seqTbl[idx - 1];
		int val_b = seqTbl[idx];
		seqTbl[idx] = val_a + val_b;
		//printf("%p %d: %d %d %d\n", seqTbl, idx - 1, val_a, val_b, seqTbl[idx]);
	}

	seqTbl = (int *)((char *)buf + 0x4040);
	//printf("%p %08X %08X\n", seqTbl, seqTbl[0], seqTbl[1]);
	for(idx = 1; idx < 1024; idx++) {
		int val_a = seqTbl[idx - 1];
		int val_b = seqTbl[idx];
		seqTbl[idx] = val_a + val_b;
		//printf("%p %d: %d %d %d\n", seqTbl, idx - 1, val_a, val_b, seqTbl[idx]);
	}



	VAL_TYPE *new_dist = (VAL_TYPE *)((char *)buf + 0x5060);
	seqTbl = (int *)(buf);
	for(i = 0; i < n; i++) {
		/* lower 11 bits */
		idx = loc[i] & 0x7FF;
		int data_idx = seqTbl[idx];
		//new_dist[data_idx] = idx;
		new_dist[data_idx] = loc[i];
		data_idx++;
		seqTbl[idx] = data_idx;
	}


	VAL_TYPE *new_src = (VAL_TYPE *)((char *)buf + 0x5060);
	seqTbl = (int *)((char *)buf + 0x2020);
	for(i = 0; i < n; i++) {
		/* middle 11 bits */
		idx = ((new_src[i] >> 11) & 0x7FF);
		int data_idx = seqTbl[idx];
		//new_dist[data_idx] = idx;
		loc[data_idx] = new_src[i];
		//loc[data_idx] = new_src[i] & 0x3FFFF;
		//printf("%d, %X, %08X, %d\n", i, idx, new_src[i], data_idx);
		data_idx++;
		seqTbl[idx] = data_idx;
	}

	//hexdump((char *)ptr, n_bytes);

	new_dist = (VAL_TYPE *)((char *)buf + 0x5060);
	seqTbl = (int *)((char *)buf + 0x4040);
	for(i = 0; i < n; i++) {
		/* upper 10 bits */
		idx = (loc[i] >> 22) & 0x3FF;
		int data_idx = seqTbl[idx];
		//new_dist[data_idx] = idx;
		new_dist[data_idx] = loc[i];
		//printf("%d, %X, %08X, %d\n", i, idx, loc[i], data_idx);
		data_idx++;
		seqTbl[idx] = data_idx;
	}

	memcpy(ptr, (char *)ptrd + 0x5060, n * sizeof(VAL_TYPE));
	//hexdump((char *)ptrd + 0x5060, n * sizeof(VAL_TYPE));

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
	allocMem(&ptrd, n_bytes + EXTRA_BYTES, hugepage_forced);

	unsigned long long steps = n_bytes / sizeof(unsigned long long);

	if (test_case < 0 ||test_case == 0) {
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

		memset(ptrd, 0, 0x5060);
		start_ns = time_ns(&ts);
		radix_sort((VAL_TYPE *) ptr, (void *)ptrd, nElems);
		delta = time_ns(&ts) - start_ns;
        printf("%p %016llX %p %016llX\n", ptr, *(unsigned long long *)ptr, (unsigned long long *)ptr + 1, *((unsigned long long *)ptr + 1));
        printf("%p %016llX %p %016llX\n", pos - 2, *(pos - 2), pos - 1, *(pos - 1));
		printf("Case %d: %ld %ld\n", 2, delta, steps);
	}


	//hexdump(ptr, n_bytes);
	//memset(ptrd, 0, 0x5060);
	//radix_sort((VAL_TYPE*)ptr, (void *)ptrd, nElems);
	//hexdump((char *)ptrd, 0x5060);
	//hexdump((char *)ptrd + 0x5060, n_bytes);


}

