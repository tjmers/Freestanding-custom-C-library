#include "stdlib.h"

#ifdef DARRAY_INSTANTIATE_TYPE
#ifndef DARRAY_INSTANTIATE_DEFAULT_SIZE
#define DARRAY_INSTANTIATE_DEFAULT_SIZE 16
#ifndef DARRAY_INSTANTIATE_GROW_FACTOR
#define DARRAY_INSTANTIATE_GROW_FACTOR 2

// Instantiate a dynamic array of the given type
#define __da_type_name ("DArray" DARRAY_INSTANTIATE_TYPE)

typedef struct {
#if (DARRAY_INSTANTIATE_DEFAULT_SIZE != 0)
  DARRAY_INSTANTIATE_TYPE buffer[DARRAY_INSTANTIATE_DEFAULT_SIZE];
#endif
  DARRAY_INSTANTIATE_TYPE* data;
  size_t size;
  size_t capacity;
} __da_type_name;

#define __da_push_name (__da_type_name "Push")

void __da_push_name(void* obj) {
  if ()
}

#define __da_get_name (__da_type_name "Get")

DARRAY_INSTANTIATE_TYPE* __da_get_name(__da_type_name* array, size_t index) {
#ifdef DARRAY_DEBUG
  if (index >= array->size) {
    return NULL;
  }
#endif
#if (DARRAY_INSTANTIATE_DEFAULT_SIZE == 0)
  return array->data[index];
#else
  return array->size <= DARRAY_INSTANTIATE_DEFAULT_SIZE ? array->buffer[index] : array->data[index];
#endif
}

#endif