// gcc -O3 -o tlbShootDownBench tlbShootDownBench.c -pthread

#define _GNU_SOURCE
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#define BUFLEN (256)


#if defined(__i386__)
static __inline__ unsigned long long __my_rdtsc(void)
{
	  unsigned long long int x;
	       __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
	            return x;
}
#elif defined(__x86_64__)
static __inline__ unsigned long long __my_rdtsc(void)
{
	  unsigned hi, lo;
	    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	      return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__aarch64__)
static __inline__ unsigned long long __my_rdtsc(void)
{
	  unsigned long long virtual_timer_value;
	    asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
	      return virtual_timer_value;
}
#else
#error "Not Supported"
#endif


typedef enum {
	size = 0,
	kps,
	mmu,
	rss,
	pss,
	shrc,
	shrd,
	pric,
	prid,
	ref,
	anon,
	lzf,
	anonp,
	shmap,
	shhu,
	prhu,
	swap,
	swpss,
	lockd,
	FIELD_LEN,
} field_type;

char *(field_name_short[]) = { "size", "kps", "mmu", "rss", "pss", "shrc",
		"shrd", "pric", "prid", "ref", "anon", "lzf", "anonp", "shmap", "shhu",
		"prhu", "swap", "swpss", "lockd", };

char *(field_name[]) = { "Size:", "KernelPageSize:", "MMUPageSize:", "Rss:",
		"Pss:", "Shared_Clean:", "Shared_Dirty:", "Private_Clean:",
		"Private_Dirty:", "Referenced:", "Anonymous:", "LazyFree:",
		"AnonHugePages:", "ShmemPmdMapped:", "Shared_Hugetlb:",
		"Private_Hugetlb:", "Swap:", "SwapPss:", "Locked:", };


int fields[FIELD_LEN];
char smap_file_name[1024];

int parse(char *filename) {
	int c, i, j;
	char linebuf[BUFSIZ];
	FILE *file;

	file = fopen(filename, "r");
	if (!file) {
		perror(filename);
		return 1;
	}

	memset(fields, 0, FIELD_LEN * sizeof(int));


	char _name[BUFSIZ];
	int _value;

	while (fgets(linebuf, sizeof(linebuf), file)) {

		//fprintf(stderr, "%s", linebuf);
		if (ferror(file)) {
			perror(filename);
			break;
		}

		memset(_name, 0, sizeof(_name));
		if (sscanf(linebuf, "%s%d", _name, &_value) != 2) {
			continue;
		}

		//fprintf(stderr, "*** %s %d\n", _name, _value);

		for (j = 0; j < FIELD_LEN; j++) {
			if (strcmp(_name, field_name[j]) == 0) {
				fields[j] += _value;
				break;
			}
		}

	}
	fclose(file);

	//for (j = 0; j < FIELD_LEN; j++) {
	//	fprintf(stderr, "*** %s %d\n", field_name[j], fields[j]);
	//}

}

#define EXPECT(thing) if (!(thing)) { perror("Failed "#thing); exit(1); }
#define MAX_PAGE_IN (1024 * 1024 * 128)
#define MIN(a,b) ((a)<(b)?(a):(b))

static void inline get_rss(const char *tag) {
	parse(smap_file_name);
	printf("%-25s ==>\t", tag); fflush(stdout);
}

static void inline get_rss_basic(const char *tag) {
	struct rusage u1;
	int ret = 0;
	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	parse(smap_file_name);
	printf("%-25s ==>\t%s:\t%lu\n", tag, field_name[rss], fields[rss]); fflush(stdout);
}


void *addr = NULL;
void *map = NULL;
void* dummy_thread(void *data) {
	int i = (int)(intptr_t)data;
	while(1) {
		((volatile unsigned char*)map)[i * 4096] = '\0';
	}
	return NULL;
}

int main(int argc, const char **argv) {

	int nthread = 1;
	if(argc > 1) {
		nthread = atoi(argv[1]);
	}
	printf("nthread set to: %d\n\n", nthread);

	size_t mysize = 128 * 1024 * 1024;
	size_t pgsize = sysconf(_SC_PAGESIZE);

	int fd = -1;
	int ret = -1;

	int madv_free_supported = 1;

	struct rusage u1, u2;
	memset(&u1, 0, sizeof(u1));
	memset(&u2, 0, sizeof(u2));
	unsigned long long clk1 = 0;
	unsigned long long clk2 = 0;

	pid_t mypid = getpid();
	sprintf(smap_file_name, "/proc/%d/smaps", mypid);

	// map it
	get_rss_basic("before mmap                    ");
	map = mmap(addr, mysize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
	get_rss_basic("after mmap                     ");
	EXPECT(map != MAP_FAILED);
	printf("Addr at:\t%p\n", map);

	// does WILLNEED do anything?  seems like no.
	ret = madvise(map, mysize, MADV_WILLNEED);
	EXPECT(ret == 0);
	((unsigned char*)map)[0] = '\0';
	printf("sleep 1 second ... please wait ... \n");
      	fflush(stdout);
	sleep(1);
	get_rss_basic("after WILLNEED                 ");

	int i, limit;
	limit = MIN(mysize, MAX_PAGE_IN);

	// walk it.  add it to sum and print at the end so the optimizer doesn't get clever.
	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("after touching                 ");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);

	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("Re-touching                    ");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);


#ifndef MADV_FREE
#define MADV_FREE 8
#warning ("Warning : MADV_FREE NOT DEFINED, FORCE DEFINE to " MADV_FREE)
#endif
	// give it back!
	// here are other flags you can pass to madvise:
	clk1 = __my_rdtsc();
	ret = madvise(map, mysize, MADV_FREE);
	clk2 = __my_rdtsc();
	if(ret != 0) {
		madv_free_supported = 0;
		fprintf(stderr, " *** MADV_FREE is not supported for this kernel\n");
	}
	printf("\tclks of MADV_FREE:\t%llu\n", clk2 - clk1);
	get_rss_basic("after FREE                     ");


	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("after madv_free re-touching    ");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);


	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("Re-touching                    ");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);


	// give it back!
	// here are other flags you can pass to madvise:
	clk1 = __my_rdtsc();
	madvise(map, mysize, MADV_FREE);
	clk2 = __my_rdtsc();
	printf("\tclks of MADV_FREE:\t%llu\n", clk2 - clk1);
	get_rss_basic("after secondary FREE           ");

	// give it back!
	// here are other flags you can pass to madvise:
	// MADV_NORMAL, MADV_RANDOM, MADV_SEQUENTIAL, MADV_WILLNEED, MADV_DONTNEED
	clk1 = __my_rdtsc();
	ret = madvise(map, mysize, MADV_DONTNEED);
	clk2 = __my_rdtsc();
	EXPECT(ret == 0);
	printf("\tclks of DONTNEED:\t%llu\n", clk2 - clk1);
	get_rss_basic("after DONTNEED                 ");

	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("after madv_dontneed re-touching");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);


	ret = getrusage(RUSAGE_SELF, &u1);
	EXPECT(ret == 0);
	clk1 = __my_rdtsc();
	for (i = 0; i < limit; ) {
		((unsigned char*)map)[i] = '\0';
		i += pgsize;
	}
	clk2 = __my_rdtsc();
	ret = getrusage(RUSAGE_SELF, &u2);
	EXPECT(ret == 0);
	get_rss("Re-touching                    ");
	printf("%s:\t%lu\tru_minflt:\t%llu\tclks:\t%llu\n", field_name[rss], fields[rss], u2.ru_minflt - u1.ru_minflt, clk2 - clk1);

	// give it back!
	// here are other flags you can pass to madvise:
	// MADV_NORMAL, MADV_RANDOM, MADV_SEQUENTIAL, MADV_WILLNEED, MADV_DONTNEED
	clk1 = __my_rdtsc();
	ret = madvise(map, mysize, MADV_DONTNEED);
	clk2 = __my_rdtsc();
	EXPECT(ret == 0);
	printf("\tclks of DONTNEED:\t%llu\n", clk2 - clk1);
	get_rss_basic("after direct DONTNEED          ");

	pthread_t pthid;
	for(i = 0; i < nthread; i++) {
		pthread_create(&pthid, NULL, &dummy_thread, (void *)(intptr_t)i);
	}

    i = 0;
    int cnt = 100000;
    long long utime = 0;
    long long stime = 0;
    long long wtime = 0;
    long ru_minflt = 0;
    double sys_pct = 0.0f;
    double us_per_mfault = 0.0f;
    double est_freq = 0.0f;
    getrusage(RUSAGE_SELF, &u1);
    clk1 = __my_rdtsc();
	while(1) {
		madvise(map, mysize, MADV_DONTNEED);
		i++;
		if(i >= 100000) {
            clk2 = __my_rdtsc();
            getrusage(RUSAGE_SELF, &u2);
            ru_minflt = u2.ru_minflt - u1.ru_minflt;
            utime = (u2.ru_utime.tv_sec - u1.ru_utime.tv_sec) * 1000000 +  (u2.ru_utime.tv_usec - u1.ru_utime.tv_usec);
            stime = (u2.ru_stime.tv_sec - u1.ru_stime.tv_sec) * 1000000 +  (u2.ru_stime.tv_usec - u1.ru_stime.tv_usec);
            wtime = (utime + stime) / (nthread + 1);
            sys_pct = (double) (stime - wtime) / (double) nthread * 100.0f / wtime;
            us_per_mfault = (double) (stime - wtime) * 1000.f / (ru_minflt + 1);
            est_freq = (double)(clk2 - clk1) / (double) wtime;
            printf("Cost %.3f Clks for last %d madvise syscall, ru_minflt %ld utime %lld stime %lld wtime %lld sys_pct %.3f%% ns_pf %.3f est_freq %.3f\n",
                (double)(clk2 - clk1) / (double)cnt, cnt, ru_minflt, utime, stime, wtime, sys_pct, us_per_mfault, est_freq);
            i = 0;
            //getrusage(RUSAGE_SELF, &u1);
            memcpy(&u1, &u2, sizeof(u1));
            clk1 = __my_rdtsc();
		}
	}
	return 0;
}
