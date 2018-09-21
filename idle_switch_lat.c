
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

#include <pthread.h>

//unsigned long long lat[128 * 1024][3];
unsigned long long a1 = 0, b1 = 0, c1 = 0;
unsigned long long a2 = 0, b2 = 0, c2 = 0;
unsigned long long a3, b3, c3;

pid_t pid, tid;

static inline long long unsigned time_ms(struct timespec *ts) {
	if (clock_gettime(CLOCK_REALTIME, ts)) {
		exit(1);
	}
}


/* this function is run by the second thread */
void *do_sleep(void *data) {

	tid = syscall(SYS_gettid);

	while(1) {
		sleep(1);
	}

	/* the function must return something - NULL will do */
	return NULL;

}



int main() {

	/* this variable is our reference to the second thread */
	pthread_t sleep_thread;
	char proc_path[128];
	struct timespec start, end;

	pid = syscall(SYS_gettid);

	/* create a second thread which executes inc_x(&x) */
	if(pthread_create(&sleep_thread, NULL, do_sleep, NULL)) {
		return -1;
	}

	while(1) {

		time_ms(&start);
		usleep(999000);
		sprintf(proc_path, "/proc/%d/schedstat", tid);
		//printf("File %s\n", proc_path);

        if(c1 % 10 == 0) {
                fflush(stdout);
        }

		FILE *fp = fopen(proc_path, "r");
		if(fp != NULL) {
			fscanf(fp, "%lld %lld %lld", &a1, &b1, &c1);
			fclose(fp);
			a3 = a1 - a2;
			b3 = b1 - b2;
			c3 = c1 - c2;
			a2 = a1;
			b2 = b1;
			c2 = c1;
			time_ms(&end);
			printf("%lld %lld %lld %lld %lld %lld %lld %lld\n",
					end.tv_sec * 1000 + end.tv_nsec / 1000000,
					start.tv_sec * 1000 + start.tv_nsec / 1000000,
					a1, b1, c1, a3, b3, c3);
		}

	}

	pthread_join(sleep_thread, NULL);


	return 0;

}
