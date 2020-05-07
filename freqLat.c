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


void get_runtime(){
	char *fname = "/proc/self/sched";
	FILE *fp = fopen(fname, "re");
	if(fp == NULL) {
		return;
	}
	char buf[256];
	buf[0] = '\0';
	while(NULL != fgets(buf, 256, fp)) {
		if(strstr(buf, "se.sum_exec_runtime") != NULL) {
		//if(strstr(buf, "se.vruntime") != NULL) {
			break;
		}
	}

	printf("%s", buf);
	fclose(fp);
}

static inline unsigned long long time_ns_res(struct timespec* const ts) {
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ts)) {
		exit(1);
	}
	return ((unsigned long long) ts->tv_sec) * 1000000000LLU
			+ (unsigned long long) ts->tv_nsec;
}


double measure_rnd_read(char **paddr, unsigned int itr1,
		unsigned long long itr2) {
	unsigned long long delta1 = 0, delta2 = 0;
	unsigned long long i = 0;
	register char **p = (char **) (*paddr);
	unsigned long long start_ns;
	unsigned long long start_ns_res;
	struct timespec ts;
	struct timespec ts_res;

	//start_ns = time_ns(&ts);
	start_ns_res = time_ns(&ts_res);
	//get_runtime();
	while (itr1-- > 0) {
		for (i = 0; i < itr2; i++) {
			RD_HUNDRED
			;
		}
	}
	//get_runtime();
	delta2 = time_ns(&ts_res) - start_ns_res;
	//delta1 = time_ns(&ts) - start_ns;
	/* avoid compiler optimization */
	*paddr = *p;
	printf("clock_gettime %lld and self %lld\n", delta1, delta2);
	return (double) delta2;
}

double measure_rdlat_user(void** pptr, unsigned long long n_bytes, unsigned long stride) {
	int ret = 0;
	unsigned long long *pos = NULL;
	void *pos_shadow = NULL;
	double delta = 0.0f;

	unsigned long long n_segs = n_bytes / stride;
	//printf("*** DEBUG: n_bytes=%lld, n_segs=%lld, stride=%d\n", n_bytes, n_segs, stride);

	ret = posix_memalign(pptr, sysconf(_SC_PAGESIZE), n_bytes);
	if (ret) {
		fprintf(stderr, "None zero ret code %d\n", ret);
		exit(EXIT_FAILURE);
	}

	// under 32bit mode, explicitly convert from 64bit int to 32 bit int is a MUST !!!
	printf("Using size: %d KiB with stride %d (%lld segs)\n",
			(int) (n_bytes / 1024UL), stride, n_segs);

	unsigned long i  = 0;
	pos_shadow = *pptr + stride * (n_segs - 1);
	pos = (unsigned long long *) (pos_shadow);
	for(i = 1; i < n_segs; i++) {
		*pos = (unsigned long long)(intptr_t)(pos_shadow - stride);
		pos_shadow -= stride;
		pos = (unsigned long long *) (pos_shadow);
	}
	*pos = (unsigned long long)(intptr_t)(*pptr + n_bytes - stride);

	delta = measure_rnd_read((char **) &pos, 1000, 1000);
	return delta;
}



int main(int argc, char *argv[]) {
	int opt = 0, ret;

	unsigned long long *pos = NULL, *ptr = NULL, n_bytes = 0;
	unsigned long long start_ns;
	struct timespec ts;

	unsigned long mem_size_per_thread_in_kb = 16;
	unsigned long stride = 128;
	double delta = 0.0f;

	while ((opt = getopt(argc, argv, "s:md:q:")) != -1) {
		switch (opt) {
		case 's':
			mem_size_per_thread_in_kb = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			stride = strtoul(optarg, NULL, 0);
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	if (mem_size_per_thread_in_kb < 4) {
		fprintf(stderr, "%d must be >= 4\n", mem_size_per_thread_in_kb);
		exit(EXIT_FAILURE);
	}

	n_bytes = mem_size_per_thread_in_kb * 1024L;

	delta = measure_rdlat_user((void**) &ptr, n_bytes, stride);
	printf("Latency = %.03f Freqency = %.03f\n", delta / (double) 1000000.0f, 4000000.0 / (double) delta);

	EXIT_SUCCESS;
}
