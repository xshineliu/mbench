#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>

#include <cstdio>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>

void trigger(int cpu, char* ptr, std::size_t sz) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  assert(pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0);
  for (std::size_t i = 0; i < sz; i += 4096) {
    *ptr = '\0';
    ptr += 4096;
  }
}

int main(int argc, char* argv[]) {
  std::size_t sz = 100;
  auto nproc = std::thread::hardware_concurrency();
  if (argc > 1) {
    sz = atoi(argv[1]);
  }
  if (argc > 2) {
    nproc = atoi(argv[2]);
  }
  std::vector<std::thread> thr;
  sz <<= 30;
  auto* ptr = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
                   -1, 0);
  assert(ptr != MAP_FAILED);
  char* cptr = static_cast<char*>(ptr);
  auto run = sz / nproc;
  run = (run >> 12) << 12;
  rusage usage_start;
  getrusage(RUSAGE_SELF, &usage_start);
  auto start = std::chrono::system_clock::now();
  printf("Use: threads=%ld\n", nproc);
  for (auto i = 0U; i < nproc; ++i) {
    thr.emplace_back(std::thread([i, cptr, run]() { trigger(i, cptr, run); }));
    cptr += run;
  }
  for (auto& t : thr) {
    t.join();
  }
  rusage usage_end;
  getrusage(RUSAGE_SELF, &usage_end);
  timeval utime;
  timeval stime;
  timersub(&usage_end.ru_utime, &usage_start.ru_utime, &utime);
  timersub(&usage_end.ru_stime, &usage_start.ru_stime, &stime);
  printf("usr: %ld.%06ld\n", utime.tv_sec, utime.tv_usec);
  printf("sys: %ld.%06ld\n", stime.tv_sec, stime.tv_usec);
  auto end = std::chrono::system_clock::now();
  printf("cost: %lu\n",
         std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                               start).count());
  return 0;
}
