#ifndef RAYCAST_MACROS_INCLUDED
#define RAYCAST_MACROS_INCLUDED

#define M_MAX(x, y) (((x)>(y))?(x):(y))
#define M_MIN(x, y) (((x)<(y))?(x):(y))
#define M_CLAMP(N, L, U) M_MAX(M_MIN(U, N), L)
#define M_INLINED static inline __attribute__((always_inline))
#define M_PACKED __attribute__((__packed__))
#define M_BIT(B) (1<<B)
#define M_UNUSED(x) (void)(x)
#define M_OPTIONAL(X) X
#define M_DISCARDABLE(X) X

// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
#define M_NARG(...) M__NARG_(_,##__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define M__NARG_(_,...) M__ARG_N(__VA_ARGS__)
#define M__ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

#endif