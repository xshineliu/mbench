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

#define RD_ONE  p = (char **)*p;
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

	start_ns = time_ns(&ts);
	do {

		__builtin_ia32_rdrand64_step(&rnd);

		tmp =
				(unsigned long long *) ((void *) addr
						+ (rnd % n_segs) * seg_size);
		if (*tmp) {
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
			seg_size, (double) delta / 1000000.0f,
			(double) n_conflict / (double) n_done);
}

double measure_rnd_read(char **paddr, unsigned int itr1,
		unsigned long long itr2) {
	unsigned long long delta1 = 0, delta2 = 0;
	unsigned long long i = 0;
	register char **p = (char **) (*paddr);
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
	delta2 = (time_us_timeval(&(r2.ru_utime)) - time_us_timeval(&(r1.ru_utime)))
			* 1000;
	/* avoid compiler optimization */
	*paddr = *p;
	//printf("%llu, %.3f\n", delta, (double)delta / (double) itr2 / (double)itr1 / 100LLU);
	if (delta2 > 0 && delta2 < delta1) {
		printf("WARN: clock_gettime %lld and getrusage %lld mismatch\n", delta1,
				delta2);
	}
	return (double) delta1;
}

double measure_rdlat_user(void** pptr, unsigned long long n_bytes,
		unsigned long stride, float rand_ratio) {
	int ret = 0;
	unsigned long long *pos = NULL;
	double delta = 0.0f;

	unsigned long long n_segs = n_bytes / stride;
	//printf("*** DEBUG: n_bytes=%lld, n_segs=%lld, stride=%d\n", n_bytes, n_segs, stride);

	ret = posix_memalign(pptr, sysconf(_SC_PAGESIZE), n_bytes);
	if (ret) {
		fprintf(stderr, "None zero ret code %d\n", ret);
		exit(EXIT_FAILURE);
	}

	// under 32bit mode, explicitly convert from 64bit int to 32 bit int is a MUST !!!
	printf("Using size: %d KiB (%f MiB) with stride %d (%lld segs)\n",
			(int) (n_bytes / 1024UL),
			((double) (n_bytes / 1024UL)) / (double) 1024.0f, stride, n_segs);

	memset((void *) *pptr, 0x0, n_bytes);
	setup_rnd(*pptr, n_bytes, stride, rand_ratio);
	pos = *pptr;

	delta = measure_rnd_read((char **) &pos, 100, 100);
	return delta;
}


double measure_rdlat_dev_mem(void** pptr, unsigned long long n_bytes,
		unsigned long stride, float rand_ratio) {

	int mem_fd;
	FILE *zone;
	/* max 16 numa node yet */
	unsigned long long pfn[16] = {0,} ;
	char buf[128];
	int node = -1;

	off_t off = 0;

	zone = fopen("/proc/zoneinfo", "r");
    if (mem_fd < 0) {
        printf("Failed to open /proc/zoneinfo (%s)\n", strerror(errno));
        return -1;
    }

    while (NULL != fgets(buf, 128, zone)) {
    	int len = strlen(buf);
    	buf[len - 1] = '\0';

    	if(sscanf(buf, "Node %d, zone   Normal", &node) > 0) {
    		/* sscanf just do a blur match, so this step is needed */
    		if((buf[len - 2]) != 'l' || node > 15) {
    			/* reset invalid node */
    			node = -1;
    			continue;
    		}

    		printf("Got Node %d, %s\n", node, buf);
    		continue;
    	}

    	/* skip any lines until a valid node is found */
		if(node < 0 || node > 15) {
			/* invalid node */
			continue;
		}

		/* node must be vaild at this point */
    	if(sscanf(buf, "   start_pfn:           %lld", &(pfn[node]))> 0) {
    		printf("Got pfn %lld, %s\n", pfn[node], buf);
    		continue;
    	}
    }
	fclose(zone);


    /* open the device */
    mem_fd = open("/dev/mem", O_RDONLY);
    if (mem_fd < 0) {
        printf("Failed to open /dev/mem (%s)\n", strerror(errno));
        return -1;
    }

    off = pfn[0] * sysconf(_SC_PAGESIZE);
    //off = 0x00100000;
    //off = 0xf0000;
    *pptr = mmap(NULL, n_bytes, PROT_READ, MAP_SHARED, mem_fd, off);

    if (*pptr == MAP_FAILED)   {
        printf("Failed to do mmap %lld bytes from %p for (%s)\n",
        		n_bytes, off, strerror(errno));
    }   else   {
        printf("Succesful mmap at %p\n", *pptr);
    }



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

	while ((opt = getopt(argc, argv, "s:md:q:")) != -1) {
		switch (opt) {
		case 's':
			mem_size_per_thread_in_kb = strtoul(optarg, NULL, 0);
			break;
		case 'm':
			scale = 1024L;
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

	if (rand_ratio < 0.80 || rand_ratio > 1.00) {
		fprintf(stderr, "rand_ratio %f must between 80 to 100.\n", rand_ratio);
		exit(EXIT_FAILURE);
	}

	n_bytes = mem_size_per_thread_in_kb * 1024L;

	delta = measure_rdlat_user((void**) &ptr, n_bytes, stride, rand_ratio);
	printf("Latency: %.03f %.03f\n", delta / (double) 1000000.0f,
			(double) delta / (double) 100 / (double) 100 / (double) 100);


	//measure_rdlat_dev_mem((void**) &ptr, n_bytes, stride, rand_ratio);

	//start_ns = time_ns(&ts);
	//delta = time_ns(&ts) - start_ns;
	EXIT_SUCCESS;
}


