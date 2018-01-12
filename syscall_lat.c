#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h> 


pid_t pid, tid;
//int fd;


long long unsigned start_ns;
struct timespec ts;

static inline long long unsigned time_ns(struct timespec* const ts) {
        if (clock_gettime(CLOCK_REALTIME, ts)) {
                exit(1);
        }
        return ((long long unsigned) ts->tv_sec) * 1000000000LLU
                        + (long long unsigned) ts->tv_nsec;
}


int main(int argc, char* argv[])
{

	long long i = 0;
	long long unsigned delta;

	pid = getpid();

        start_ns = time_ns(&ts);
	for (  i = 0; i < (10*1000*1000); i++)
	{
    //pid_t getpid(void);
		//pid = getpid();
		tid = syscall(SYS_gettid);
		//fd = open("/xxxxx", O_APPEND);
	}
		
        delta = time_ns(&ts) - start_ns;
	printf("%d - %lld:\t%lld\n", pid, i, delta);

}

