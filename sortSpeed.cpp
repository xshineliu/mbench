

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>


#define N ((1000000UL))
#define TYPE_LEN (sizeof(int))
#define DEF_HUGE_PAGE_SIZE ((0x200000))


void *ptr = NULL;
unsigned int *p = NULL;


int main(int argc, char* argv[])
{

        unsigned long long *pos;
        unsigned long long i;
        unsigned long long start_ns;
        struct timespec ts;

        int ret = 0;

        int hugepage_forced = 1;
        int hugepage_allocate = 0;
        unsigned long long n_bytes = TYPE_LEN * N;


        if(hugepage_forced) {
                ptr = mmap(NULL, ((n_bytes < DEF_HUGE_PAGE_SIZE) ? DEF_HUGE_PAGE_SIZE : n_bytes),
                         PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0) ;
                if(ptr != MAP_FAILED) {
                        printf("HugePage allocated at pointer %p with size %llX\n", ptr, n_bytes);
                        hugepage_allocate = 1;
                } else {
                        printf("HugePage allocated Failed: %d\n", ptr);
                }
        }


        if(!hugepage_allocate) {
                ret = posix_memalign((void **)&ptr, sysconf(_SC_PAGESIZE), n_bytes);
                if (ret) {
                        fprintf(stderr,"None zero ret code %d\n", ret);
                        exit(EXIT_FAILURE);
                }
                printf("Normal page allocated at pointer %p with size %llX,\n\t" \
                        " * Please manually disable the Transparent Huge Page if needed.\n", ptr, n_bytes);
        }



        unsigned long long steps = n_bytes / sizeof(unsigned long long);
        pos = (unsigned long long *)ptr;

        for(i = 0; i < steps; i++) {
                __builtin_ia32_rdrand64_step(pos);
                pos++;
                //printf("%llX\n", *pos);
        }

        printf("%p %llX\n", pos - 1, *(pos - 1));

}
