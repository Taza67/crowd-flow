/* cf_compat.h — CPU/CUDA inline. */

#ifndef CF_COMPAT_H
#define CF_COMPAT_H

#ifdef __CUDACC__
#define CF_INLINE __host__ __device__ static inline
#else
#define CF_INLINE static inline
#endif

#endif
