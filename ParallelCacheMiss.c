/*
 * gcc -DPRL=8 -O3  -o  ParallelLLCMiss ParallelLLCMiss.c -mrdrnd
 * ./ParallelLLCMiss -m -s 1024 -d 256 -q 99
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>
/* Linux Only */
#define DEF_HUGE_PAGE_SIZE (2 * 1024 * 1024)

//#define RD_ONE  p = (char **)*p;
#if PRL == 16
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2; p3 = (char **)*p3; p4 = (char **)*p4; p5 = (char **)*p5; p6 = (char **)*p6; p7 = (char **)*p7; p8 = (char **)*p8; p9 = (char **)*p9; p10 = (char **)*p10; p11 = (char **)*p11; p12 = (char **)*p12; p13 = (char **)*p13; p14 = (char **)*p14; p15 = (char **)*p15; p16 = (char **)*p16;
#elif PRL == 12
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2; p3 = (char **)*p3; p4 = (char **)*p4; p5 = (char **)*p5; p6 = (char **)*p6; p7 = (char **)*p7; p8 = (char **)*p8; p9 = (char **)*p9; p10 = (char **)*p10; p11 = (char **)*p11; p12 = (char **)*p12;
#elif PRL == 8
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2; p3 = (char **)*p3; p4 = (char **)*p4; p5 = (char **)*p5; p6 = (char **)*p6; p7 = (char **)*p7; p8 = (char **)*p8;
#elif PRL == 6
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2; p3 = (char **)*p3; p4 = (char **)*p4; p5 = (char **)*p5; p6 = (char **)*p6;
#elif PRL == 4
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2; p3 = (char **)*p3; p4 = (char **)*p4;
#elif PRL == 2
#define RD_ONE  p1 = (char **)*p1; p2 = (char **)*p2;
#else
#define RD_ONE  p1 = (char **)*p1;
#endif

#define RD_FIVE RD_ONE RD_ONE RD_ONE RD_ONE RD_ONE
#define RD_TEN  RD_FIVE RD_FIVE
#define RD_FIFTY        RD_TEN RD_TEN RD_TEN RD_TEN RD_TEN
#define RD_HUNDRED      RD_FIFTY RD_FIFTY

static inline unsigned long long time_us_timeval(struct timeval* const tv) {
	return ((unsigned long long) tv->tv_sec) * 1000000LLU
			+ (unsigned long long) tv->tv_usec;
}

static inline unsigned long long time_ns(struct timespec* const ts) {
	if (clock_gettime(CLOCK_REALTIME, ts)) {
		exit(1);
	}
	return ((unsigned long long) ts->tv_sec) * 1000000000LLU
			+ (unsigned long long) ts->tv_nsec;
}

void setup_rnd(unsigned long long *addr, unsigned long long size, int seg_size,
		float rand_ratio) {

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

	// clear the addr, page fault
	start_ns = time_ns(&ts);
	for ( long long i = 0; i < n_segs; i++) {
		*(unsigned long long **) ((void *) addr + i * seg_size) = addr;
	}
	delta = time_ns(&ts) - start_ns;
	printf("Page fault take %.03f ms\n",  (double) delta / 1000000.0f);


	start_ns = time_ns(&ts);
	do {

		//__builtin_ia32_rdrand64_step(&rnd);
		rnd = random();

		tmp =
				(unsigned long long *) ((void *) addr
						+ (rnd % n_segs) * seg_size);
		if (*tmp != (uintptr_t)addr) {
			// already has data, conflict, try next?
			// for those size > 256M, will consider rand_ratio
			if ((n_done > 10000) && (n_done > rand_ratio * n_segs)) {
				break;
			} else {
				// for those size <= 256M, or size > 256M but <= rand_ratio, try next
				n_conflict++;
				continue;
			}
		}

		// adaptive for 32bit mode
		*pos = (uintptr_t) tmp;
		pos = tmp;
		n_done++;
	} while (n_done < n_segs);

	// make a ring
	*pos = (uintptr_t) addr;

	delta = time_ns(&ts) - start_ns;
	printf("Set up %lld x %lld in %.03f ms with %.03f times tries\n", n_segs,
			seg_size, (double) delta / 1000000.0f, (double) n_conflict / (double) n_done);
}

double measure_rnd_read(char **paddr, size_t stride, unsigned int itr1, unsigned long long itr2) {
	unsigned long long delta1 = 0, delta2 = 0;
	unsigned long long i = 0;
	register char **p1 = (char **) (*paddr);
	register char **p2 = (char **) (*paddr + stride);
	register char **p3 = (char **) (*paddr + 2 * stride);
	register char **p4 = (char **) (*paddr + 3 * stride);
	register char **p5 = (char **) (*paddr + 4 * stride);
	register char **p6 = (char **) (*paddr + 5 * stride);
	register char **p7 = (char **) (*paddr + 6 * stride);
	register char **p8 = (char **) (*paddr + 7 * stride);
	register char **p9 = (char **) (*paddr + 8 * stride);
	register char **p10 = (char **) (*paddr + 9 * stride);
	register char **p11 = (char **) (*paddr + 10 * stride);
	register char **p12 = (char **) (*paddr + 11 * stride);
	register char **p13 = (char **) (*paddr + 12 * stride);
	register char **p14 = (char **) (*paddr + 13 * stride);
	register char **p15 = (char **) (*paddr + 14 * stride);
	register char **p16 = (char **) (*paddr + 15 * stride);
	unsigned long long start_ns;
	struct timespec ts;
	struct rusage r1, r2;


	memset(&r1, 0, sizeof(r1));
	memset(&r1, 0, sizeof(r2));

	getrusage(RUSAGE_THREAD, &r1);
	start_ns = time_ns(&ts);
	while (itr1-- > 0) {
		for (i = 0; i < itr2; i++) {
			RD_HUNDRED
			;
		}
	}

	delta1 = time_ns(&ts) - start_ns;
	getrusage(RUSAGE_THREAD, &r2);
	delta2 = (time_us_timeval(&(r2.ru_utime)) - time_us_timeval(&(r1.ru_utime))) * 1000;
	/* avoid compiler optimization */
	*paddr += (intptr_t)*p1 + (intptr_t)*p2 + (intptr_t)*p3 + (intptr_t)*p4 + (intptr_t)*p5 + (intptr_t)*p6 + (intptr_t)*p7 + (intptr_t)*p8 ;
	*paddr += (intptr_t)*p9 + (intptr_t)*p10 + (intptr_t)*p11 + (intptr_t)*p12 + (intptr_t)*p13 + (intptr_t)*p14 + (intptr_t)*p15 + (intptr_t)*p16 ;
	//printf("%llu, %.3f\n", delta, (double)delta / (double) itr2 / (double)itr1 / 100LLU);
	if (delta2 > 0 && delta2 < delta1) {
		printf("WARN: clock_gettime %lld and getrusage %lld mismatch\n", delta1, delta2);
	}
	return (double) delta1;
}

int hugepage_forced = 0;
void* alloc_memory(size_t n_bytes) {
	void *ptr = NULL;
	if(hugepage_forced) {
		ptr = mmap(NULL, ((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
                         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0) ;
		if(ptr != MAP_FAILED) {
			printf("HugePage allocated at pointer %p\n", ptr);
			return ptr;
		} else {
                        printf("HugePage allocated Failed: %p\n", ptr);
		}
	}
	// fall back to normal page allocation
	int ret = 0;
	ret = posix_memalign((void **)&ptr, sysconf(_SC_PAGESIZE), n_bytes);
	if(ret) {
		fprintf(stderr,"Normal page allocation failure due to None zero ret code %d\n", ret);
		exit(EXIT_FAILURE);
	} else {
		printf("Normal page allocated at pointer %p\n", ptr);
	}
	return ptr;
}


double measure_rdlat_user(void** pptr, unsigned long long n_bytes,
		unsigned long stride, float rand_ratio) {
	int ret = 0;
	unsigned long long *pos = NULL;
	double delta = 0.0f;

	unsigned long long n_segs = n_bytes / stride;
	//printf("*** DEBUG: n_bytes=%lld, n_segs=%lld, stride=%d\n", n_bytes, n_segs, stride);

	*pptr = alloc_memory(n_bytes);

	// under 32bit mode, explicitly convert from 64bit int to 32 bit int is a MUST !!!
	printf("Using size: %d KiB (%f MiB) with stride %d (%lld segs)\n", (int) (n_bytes / 1024UL),
			((double) (n_bytes / 1024UL)) / (double) 1024.0f, stride, n_segs);

	//memset((void *) *pptr, 0x0, n_bytes);
	setup_rnd(*pptr, n_bytes, stride, rand_ratio);
	pos = *pptr;

	int cnt1 = 100;
	unsigned long long cnt2 = 10000;
	delta = measure_rnd_read((char **) &pos, stride, cnt1, cnt2);
	printf("Cost Each with parallel load %d: %.03f\n", PRL, delta / (double) cnt1 / (double) cnt2 / (double) 100);

	return delta;
}



int main(int argc, char *argv[]) {
	int opt = 0, ret;

	unsigned long long *pos = NULL, *ptr = NULL, n_bytes = 0;
	unsigned long long start_ns;
	struct timespec ts;

	unsigned long mem_size_per_thread_in_kb = 4;
	unsigned int scale = 1;
	unsigned long stride = 64;
	float rand_ratio = 0.99f;
	double delta = 0.0f;

	while ((opt = getopt(argc, argv, "s:mld:q:")) != -1) {
		switch (opt) {
		case 's':
			mem_size_per_thread_in_kb = strtoul(optarg, NULL, 0);
			break;
		case 'm':
			scale = 1024L;
			break;
		case 'l':
			hugepage_forced = 1;
			break;
		case 'd':
			stride = strtoul(optarg, NULL, 0);
			break;
		case 'q':
			rand_ratio = (double) (strtoul(optarg, NULL, 0)) * 0.01f;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	mem_size_per_thread_in_kb *= scale;

	if (mem_size_per_thread_in_kb < 4) {
		fprintf(stderr, "%d must be >= 4\n", mem_size_per_thread_in_kb);
		exit(EXIT_FAILURE);
	}

	if (rand_ratio < 0.01 || rand_ratio > 1.00) {
		fprintf(stderr, "rand_ratio %f must between 80 to 100.\n", rand_ratio);
		exit(EXIT_FAILURE);
	}

	n_bytes = mem_size_per_thread_in_kb * 1024L;

	delta = measure_rdlat_user((void**) &ptr, n_bytes, stride, rand_ratio);

	EXIT_SUCCESS;
}
