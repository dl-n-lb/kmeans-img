#ifndef VEC_H_
#define VEC_H_

#ifndef NDEBUG
#define SAFE_INDEX
#endif //NDEBUG

#ifndef VEC_ASSERT
#include <assert.h>
#define VEC_ASSERT assert
#endif // VEC_ASSERT

#ifndef VEC_MALLOC
#include <stdlib.h>
#define VEC_MALLOC malloc
#endif // VEC_MALLOC

#ifndef VEC_REALLOC
#include <stdlib.h>
#define VEC_REALLOC realloc
#endif // VEC_REALLOC

#ifndef VEC_MEMCPY
#include <string.h>
#define VEC_MEMCPY memcpy
#endif // VEC_MEMCPY

typedef struct {
  char *data;
  size_t len;
} arr_t;

typedef struct {
  const char *data;
  size_t len;
} view_t;

typedef struct {
  union {
    struct {
      char *data;
      size_t len;
    };
    view_t slice;
    arr_t arr;
  };
  size_t cap;
} vec_t;

#define VEC_GROW_RATE 2

#define vec_new(T, sz)				\
  (vec_t) {					\
    .data = VEC_MALLOC(sizeof(T) * sz),		\
    .len = 0,					\
    .cap = sz,				        \
  }

#define arr_new(T, sz)				\
  (arr_t) {					\
    .data = VEC_MALLOC(sizeof(T) * sz),		\
    .len = sz,					\
  }

#define arr_from(T, c_arr, sz)			\
  (arr_t) { .data = (char *)c_arr, .len = sz }

#define vec_from(T, c_arr, sz) (vec_t) {				\
    .data = (char *)c_arr, .len = sz, .cap = sz }

vec_t _vec_from_const(char *const_arr, size_t sz, size_t elem_sz);

#define vec_from_const(T, const_arr)					\
  _vec_from_const((char *)(const_arr), sizeof((const_arr)), sizeof((const_arr)[0]))

#define vec_push(T, vec, val) do {					\
    if ((vec).len + 1 >= (vec).cap) {					\
      (vec).cap *= VEC_GROW_RATE;					\
      (vec).data = VEC_REALLOC((vec).data, sizeof(T) * (vec).cap);	\
      VEC_ASSERT((vec).data && "REALLOC FAILED WHAT LOL TROLLED");	\
    }									\
    ((T*)(vec).data)[(vec).len++] = (val);				\
  } while(0)

#ifdef SAFE_INDEX
#define arr_at(T, arr, idx) (assert((idx) >= 0 && (idx) < (arr).len),	\
			     ((T*)(arr).data)[(idx)])
#define vec_at(T, vec, idx) arr_at(T, (vec).arr, idx)
#define arr_ref(T, arr, idx) (assert((idx) >= 0 && (idx) < (arr).len),	\
			      &((T*)(arr).data)[(idx)])
#define vec_ref(T, vec, idx) arr_ref(T, (vec).arr, idx)
#else
#define arr_at(T, arr, idx) ((T*)(arr).data)[(idx)]
#define vec_at(T, vec, idx) arr_at(T, (vec).arr, idx)
#define arr_ref(T, arr, idx) &((T*)(arr).data)[(idx)]
#define vec_ref(T, vec, idx) arr_ref(T, (vec).arr, idx)
#endif // SAFE_INDEX

#define arr_for_each(T, arr, it_name)					\
  for(T* it_name = (T*)(arr).data;					\
      it_name < (T *)((arr).data + sizeof(T) * (arr).len); ++it_name)

#define vec_for_each(T, vec, it_name) arr_for_each(T, (vec).arr, it_name)

#define vec_free(vec) (free(vec.data), vec.len = 0, vec.cap = 0)
#define arr_free(arr) (free(arr.data), arr.len = 0)


// TODO:
#ifdef STRING_TYPES
typedef sstr_t arr_t;
typedef dstr_t vec_t;

#define sstr_at(sstr, idx) arr_at(char, sstr, idx)
#define dstr_at(dstr, idx) vec_at(char, dstr, idx)
#endif

#endif // VEC_H_
#ifdef VEC_IMPL

vec_t _vec_from_const(char *const_arr, size_t sz, size_t elem_sz) {
  char *data = malloc(sz);
  VEC_MEMCPY(data, const_arr, sz);
  return (vec_t) {
    .data = data,
    .len = sz/elem_sz,
    .cap = sz/elem_sz
  };
}

#endif // VEC_IMPL
