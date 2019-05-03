#ifndef _PERFOSCOPE_COMMON_HPP_
#define _PERFOSCOPE_COMMON_HPP_

#ifndef USING_MPIC
#include <cstdlib>
#endif

#include <time.h>
#include <sys/time.h>

namespace perfoscope_internal {

typedef timespec real_time_t;

inline double difftime(const timespec &end, const timespec &start) {
//  time_t sec = end.tv_sec - start.tv_sec;
//  long nsec = end.tv_nsec - start.tv_nsec;
//  return sec+nsec*1e-9;
  timespec result;
  if((end.tv_nsec - start.tv_nsec) < 0) {
    result.tv_sec = end.tv_sec - start.tv_sec - 1;
    result.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
  } else {
    result.tv_sec = end.tv_sec - start.tv_sec;
    result.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return double(result.tv_sec)+double(result.tv_nsec)*1e-9;
}

inline real_time_t get_real_time() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts;
}

inline int iproc() {
#ifdef USING_MPIC
  int ip;
  MPI_Comm_rank(MPI_COMM_WORLD, &ip);
  return ip;
#else
  return 0;
#endif
}

inline int nproc() {
#ifdef USING_MPIC
  int np;
  MPI_Comm_size(MPI_COMM_WORLD, &np);
  return np;
#else
  return 1;
#endif
}

inline void abort(int error_code) {
#ifdef USING_MPIC
  MPI_Abort(MPI_COMM_WORLD, error_code);
#else
  std::exit(error_code);
#endif
}

}

#endif // #ifndef _PERFOSCOPE_COMMON_HPP_
