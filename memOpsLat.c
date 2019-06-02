// -march=core-avx2 -qopt-streaming-stores=always -qopt-prefetch -fopenmp -Ofast

# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <omp.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

#ifndef MADV_FREE
#define MADV_FREE	8
#endif

unsigned long long STREAM_ARRAY_SIZE = 80 * 1000 * 1000;

unsigned long long NTIMES = 10;

#ifndef OFFSET
#   define OFFSET	0
#endif


# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

static STREAM_TYPE	*a,
			*b,
			*c;


static double	bytes[4];

extern double mysecond();
extern void checkSTREAMresults();
#ifdef TUNED
extern void tuned_STREAM_Copy();
extern void tuned_STREAM_Scale(STREAM_TYPE scalar);
extern void tuned_STREAM_Add();
extern void tuned_STREAM_Triad(STREAM_TYPE scalar);
#endif
#ifdef _OPENMP
extern int omp_get_num_threads();
#endif

#define DEF_HUGE_PAGE_SIZE (2 * 1024 * 1024)

int hugepage_forced = 0;
void * alloc_memory(size_t n_bytes) {
	void *ptr = NULL;
	if(hugepage_forced) {
		ptr = mmap(NULL, ((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
                         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0) ;
		if(ptr != MAP_FAILED) {
			printf("HugePage allocated at pointer %p\n", ptr);
			return ptr;
		} else {
                        printf("HugePage allocated Failed: %d\n", ptr);
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


int
main(int argc, char* argv[])
    {
    int			quantum, checktick();
    int			BytesPerWord;
    int			k;
    ssize_t		j;
    STREAM_TYPE		scalar;
    //double		t, times[4][NTIMES];
    int opt;


    while((opt = getopt(argc, argv, "ln:r:")) != -1){
        switch(opt) {
            case 'n':
                STREAM_ARRAY_SIZE = strtoull(optarg, NULL, 0);
                break;
            case 'l':
                hugepage_forced = 1;
                break;
            case 'r':
		NTIMES = strtoull(optarg, NULL, 0);
		break;
            default:
                ;
                // no action
        }
    }
    // GNU C extension
    double		t, times[4][NTIMES];

    bytes[0] = 2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE;
    bytes[1] = 2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE;
    bytes[2] = 3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE;
    bytes[3] = 3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE;

    /* --- SETUP --- determine precision and check timing --- */

    BytesPerWord = sizeof(STREAM_TYPE);
    printf(HLINE);
    printf("Array size = %llu (elements), Offset = %d (elements)\n" , (unsigned long long) STREAM_ARRAY_SIZE, OFFSET);
    printf("Memory per array = %.1f MiB (= %.1f GiB).\n",
	BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0),
	BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0/1024.0));
    printf("Total memory required = %.1f MiB (= %.1f GiB).\n",
	(3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.),
	(3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024./1024.));

#ifdef _OPENMP
    printf(HLINE);
#pragma omp parallel
    {
#pragma omp master
	{
	    k = omp_get_num_threads();
	    printf ("Number of Threads requested = %i\n",k);
        }
    }
#endif

#ifdef _OPENMP
	k = 0;
#pragma omp parallel
#pragma omp atomic
		k++;
    printf ("Number of Threads counted = %i\n",k);
#endif

    int len = sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE;
    printf("Try to Allocate Memory with size %ld ...\n", len);
    a = alloc_memory(sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE);
    b = alloc_memory(sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE);
    c = alloc_memory(sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE);

    t = mysecond();
    /* Get initial value for system clock. */
#pragma omp parallel for
    for (j=0; j<STREAM_ARRAY_SIZE; j++) {
	    a[j] = 1.0;
	    b[j] = 2.0;
	    c[j] = 0.0;
	}
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Filling cost %d microseconds\n", (int) t);


    printf("\n");
    t = mysecond();
    //free(a);
    madvise((void *)a, len, MADV_FREE);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Pre Free cost %d microseconds\n", (int) t);

    t = mysecond();
    //free(b);
    madvise((void *)b, len, MADV_FREE);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Pre Free cost %d microseconds\n", (int) t);


    t = mysecond();
    //free(c);
    madvise((void *)c, len, MADV_FREE);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Pre Free cost %d microseconds\n", (int) t);

    printf("\n");
    t = mysecond();
    free(a);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Free cost %d microseconds\n", (int) t);

    t = mysecond();
    free(b);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Free cost %d microseconds\n", (int) t);


    t = mysecond();
    free(c);
    t = 1.0E6 * (mysecond() - t);
    printf("Memory Free cost %d microseconds\n", (int) t);


    printf(HLINE);
    return 0;
}

# define	M	20

int
checktick()
    {
    int		i, minDelta, Delta;
    double	t1, t2, timesfound[M];

/*  Collect a sequence of M unique time values from the system. */

    for (i = 0; i < M; i++) {
	t1 = mysecond();
	while( ((t2=mysecond()) - t1) < 1.0E-6 )
	    ;
	timesfound[i] = t1 = t2;
	}

/*
 * Determine the minimum difference between these M values.
 * This result will be our estimate (in microseconds) for the
 * clock granularity.
 */

    minDelta = 1000000;
    for (i = 1; i < M; i++) {
	Delta = (int)( 1.0E6 * (timesfound[i]-timesfound[i-1]));
	minDelta = MIN(minDelta, MAX(Delta,0));
	}

   return(minDelta);
    }



/* A gettimeofday routine to give access to the wall
   clock timer on most UNIX-like systems.  */

#include <sys/time.h>

double mysecond()
{
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
