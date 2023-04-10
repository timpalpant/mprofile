#ifndef MPROFILE_SRC_PLATFORM_H_
#define MPROFILE_SRC_PLATFORM_H_

#if defined(__x86_64__) || defined(_M_AMD64)
#define CPU_PAUSE __builtin_ia32_pause()
#elif defined(__arm__) || defined(__aarch64__)
#define CPU_PAUSE __asm__ __volatile__("yield")
#endif

#ifndef CPU_PAUSE
#error "This CPU does not support yield or pause instructions for spinlock"
#endif


#endif
