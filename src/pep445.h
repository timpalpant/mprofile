#ifndef MPROFILE_SRC_PEP445_H_
#define MPROFILE_SRC_PEP445_H_

// Unpatched Python, forward declare the necessary structs.
// User must patch Python to be able to use the module at runtime.
typedef enum {
  PYMEM_DOMAIN_RAW,
  PYMEM_DOMAIN_MEM,
  PYMEM_DOMAIN_OBJ
} PyMemAllocatorDomain;

typedef struct {
  void *ctx;
  void *(*malloc)(void *ctx, size_t size);
  void *(*realloc)(void *ctx, void *ptr, size_t new_size);
  void (*free)(void *ctx, void *ptr);
} PyMemAllocator;

void PyMem_GetAllocator(PyMemAllocatorDomain domain, PyMemAllocator *allocator);
void PyMem_SetAllocator(PyMemAllocatorDomain domain, PyMemAllocator *allocator);

#endif  // MPROFILE_SRC_PEP445_H_
