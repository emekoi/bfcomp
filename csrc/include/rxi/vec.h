/**
 * Copyright (c) 2014 rxi
 * Copyright (c) 2021 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VEC_VERSION "0.3.0"

#ifndef VEC_ALLOC
#define VEC_ALLOC(ptr, sz) realloc(ptr, sz)
#endif

#ifndef VEC_FREE
#define VEC_FREE(ptr) free(ptr)
#endif

#define vec_t(T)                                                               \
  struct {                                                                     \
    uint8_t borrowed;                                                          \
    size_t length, capacity;                                                   \
    T *data;                                                                   \
  }

#define vec_unpack_(v)                                                         \
  (uint8_t **)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data),    \
      &(v)->borrowed

#define vec_init(v) memset((v), 0, sizeof(*(v)))

#define vec_borrowed(ptr, len, cap)                                            \
  { .borrowed = 1, .length = len, .capacity = cap, .data = ptr }

#define vec_owned(ptr, len, cap)                                               \
  {                                                                            \
    .borrowed = 0, .length = len, .capacity = cap,                             \
    .data =                                                                    \
        memcpy(VEC_ALLOC(NULL, cap * sizeof(*ptr)), ptr, len * sizeof(*ptr))   \
  }

#define vec_static(static_data)                                                \
  {                                                                            \
    .borrowed = 1, .length = sizeof(static_data),                              \
    .capacity = sizeof(static_data), .data = static_data                       \
  }

#define vec_borrowed_static(static_data)                                       \
  {                                                                            \
    .borrowed = 1, .length = 0, .capacity = sizeof(static_data),               \
    .data = static_data                                                        \
  }

#define vec_alloca(T, len)                                                     \
  {                                                                            \
    .borrowed = 1, .length = 0, .capacity = len,                               \
    .data = alloca(sizeof(T) * len)                                            \
  }

#define vec_deinit(v)                                                          \
  ((v)->borrowed ? (void)(v) : VEC_FREE((v)->data), vec_init(v))

#define vec_copy(v)                                                            \
  {                                                                            \
    .borrowed = 0, .length = (v)->length, .capacity = (v)->capacity,           \
    .data = memcpy(VEC_ALLOC(NULL, (v)->capacity * sizeof(*(v)->data)),        \
                   (v)->data, (v)->length * sizeof(*(v)->data))                \
  }

#define vec_slice(v, start, end)                                               \
  {                                                                            \
    .borrowed = 1, .length = end - start, .capacity = end - start,             \
    .data = (end - start) <= (v)->capacity ? (v)->data + start : NULL          \
  }

#define vec_push(v, val)                                                       \
  (vec_expand_(vec_unpack_(v)) ? -1 : ((v)->data[(v)->length++] = (val), 0))

#define vec_pop(v) (v)->data[--(v)->length]

#define vec_fill(v, val, count)                                                \
  do {                                                                         \
    if (vec_reserve_po2_(vec_unpack_(v), (v)->length + (count)) != 0)          \
      break;                                                                   \
                                                                               \
    for (size_t i__ = 0; i__ < (count); i__++)                                 \
      (v)->data[(v)->length++] = (val);                                        \
  } while (0)

#define vec_splice(v, start, count)                                            \
  (vec_splice_(vec_unpack_(v), start, count), (v)->length -= (count))

#define vec_swapsplice(v, start, count)                                        \
  (vec_swapsplice_(vec_unpack_(v), start, count), (v)->length -= (count))

#define vec_insert(v, idx, val)                                                \
  (vec_insert_(vec_unpack_(v), idx, 1)                                         \
       ? -1                                                                    \
       : ((v)->data[idx] = (val), (v)->length++, 0))

#define vec_insertarr(v, idx, arr, count)                                      \
  do {                                                                         \
    if (vec_insert_(vec_unpack_(v), idx, count) != 0)                          \
      break;                                                                   \
    (v)->length += count;                                                      \
    for (size_t i__ = 0; i__ < (count); i__++) {                               \
      (v)->data[idx + i__] = (arr)[i__];                                       \
    }                                                                          \
  } while (0)

#define vec_insert_as_bytes(vec, idx, v)                                       \
  vec_insertarr(vec, idx, (uint8_t *)v, sizeof(*v))

#define vec_insert_str(v, idx, str) vec_insertarr(v, idx, str, strlen(str))

#define vec_insert_arr(v, idx, arr) vec_insertarr(v, idx, arr, sizeof(arr))

#define vec_sort(v, fn) qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)

#define vec_swap(v, idx1, idx2) vec_swap_(vec_unpack_(v), idx1, idx2)

#define vec_truncate(v, len)                                                   \
  ((v)->length = (len) < (v)->length ? (len) : (v)->length)

#define vec_clear(v) ((v)->length = 0)

#define vec_first(v) (v)->data[0]

#define vec_last(v) (v)->data[(v)->length - 1]

#define vec_reserve(v, n) vec_reserve_(vec_unpack_(v), n)

#define vec_compact(v) vec_compact_(vec_unpack_(v))

#define vec_pusharr(v, arr, count)                                             \
  do {                                                                         \
    if (vec_reserve_po2_(vec_unpack_(v), (v)->length + (count)) != 0)          \
      break;                                                                   \
    for (size_t i__ = 0; i__ < (count); i__++) {                               \
      (v)->data[(v)->length++] = (arr)[i__];                                   \
    }                                                                          \
  } while (0)

#define vec_push_as_bytes(vec, v) vec_pusharr(vec, (uint8_t *)v, sizeof(*v))

#define vec_push_str(v, str) vec_pusharr(v, str, strlen(str))

#define vec_push_arr(v, arr) vec_pusharr(v, arr, sizeof(arr))

#define vec_extend(v, v2) vec_pusharr((v), (v2)->data, (v2)->length)

#define vec_find(v, val, idx)                                                  \
  do {                                                                         \
    for (*(idx) = 0; (size_t)(*(idx)) < (v)->length; (*(idx))++) {             \
      if ((v)->data[*(idx)] == (val))                                          \
        break;                                                                 \
    }                                                                          \
    if ((size_t)(*(idx)) == (v)->length)                                       \
      *(idx) = -1;                                                             \
  } while (0)

#define vec_bsearch(v, key, idx, fn)                                           \
  do {                                                                         \
    void *ptr =                                                                \
        bsearch((key), (v)->data, (v)->length, sizeof(*(v)->data), fn);        \
    *(idx) =                                                                   \
        ptr ? (ssize_t)((ptr - (void *)(v)->data) / sizeof(*(v)->data)) : -1;  \
  } while (0)

#define vec_remove(v, val)                                                     \
  do {                                                                         \
    ssize_t idx__;                                                             \
    vec_find(v, val, &idx__);                                                  \
    if (idx__ != -1)                                                           \
      vec_splice(v, idx__, 1);                                                 \
  } while (0)

#define vec_reverse(v)                                                         \
  do {                                                                         \
    int i__ = (v)->length / 2;                                                 \
    while (i__--) {                                                            \
      vec_swap((v), i__, (v)->length - (i__ + 1));                             \
    }                                                                          \
  } while (0)

#define vec_for(v, var, i)                                                     \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           size_t i;                                                           \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {0, (__typeof__((v)->data[0])){0}};                          \
         iter.i < (v)->length && ((iter.var = (v)->data[iter.i]), 1);          \
         ++iter.i)

#define vec_for_ptr(v, var, i)                                                 \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           size_t i;                                                           \
           __typeof__((v)->data[0]) *var;                                      \
         } iter = {0, NULL};                                                   \
         iter.i < (v)->length && ((iter.var = &(v)->data[iter.i]), 1);         \
         ++iter.i)

#define vec_for_rev(v, var, i)                                                 \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           size_t i;                                                           \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {(v)->length, (__typeof__((v)->data[0])){0}};                \
         iter.i > 0 && ((iter.var = (v)->data[iter.i - 1]), 1); --iter.i)

#define vec_for_rev_ptr(v, var, i)                                             \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           size_t i;                                                           \
           __typeof__((v)->data[0]) *var;                                      \
         } iter = {(v)->length, NULL};                                         \
         iter.i > 0 && ((iter.var = &(v)->data[iter.i - 1]), 1); --iter.i)

size_t vec_expand_(uint8_t **data, size_t *length, size_t *capacity,
                   size_t memsz, uint8_t *borrowed);
size_t vec_reserve_(uint8_t **data, size_t *length, size_t *capacity,
                    size_t memsz, uint8_t *borrowed, size_t n);
size_t vec_reserve_po2_(uint8_t **data, size_t *length, size_t *capacity,
                        size_t memsz, uint8_t *borrowed, size_t n);
size_t vec_compact_(uint8_t **data, size_t *length, size_t *capacity,
                    size_t memsz, uint8_t *borrowed);
size_t vec_insert_(uint8_t **data, size_t *length, size_t *capacity,
                   size_t memsz, uint8_t *borrowed, size_t idx, size_t count);
void vec_splice_(uint8_t **data, size_t *length, size_t *capacity, size_t memsz,
                 uint8_t *borrowed, size_t start, size_t count);
void vec_swapsplice_(uint8_t **data, size_t *length, size_t *capacity,
                     size_t memsz, uint8_t *borrowed, size_t start,
                     size_t count);
void vec_swap_(uint8_t **data, size_t *length, size_t *capacity, size_t memsz,
               uint8_t *borrowed, size_t idx1, size_t idx2);
#endif

#ifdef VEC_IMPL
#include <string.h>

size_t vec_expand_(uint8_t **data, size_t *length, size_t *capacity,
                   size_t memsz, uint8_t *borrowed) {
  if (*length + 1 > *capacity) {
    size_t n = (*capacity == 0) ? 1 : *capacity << 1;
    void *ptr = VEC_ALLOC(*borrowed ? NULL : *data, n * memsz);
    if (ptr == NULL)
      return -1;
    if (*borrowed)
      memcpy(ptr, *data, *length);
    *data = ptr;
    *capacity = n;
    *borrowed = 0;
  }
  return 0;
}

size_t vec_reserve_(uint8_t **data, size_t *length, size_t *capacity,
                    size_t memsz, uint8_t *borrowed, size_t n) {
  (void)length;
  if (n > *capacity) {
    void *ptr = VEC_ALLOC(*borrowed ? NULL : *data, n * memsz);
    if (ptr == NULL)
      return -1;
    if (*borrowed)
      memcpy(ptr, *data, *length);
    *data = ptr;
    *capacity = n;
    *borrowed = 0;
  }
  return 0;
}

size_t vec_reserve_po2_(uint8_t **data, size_t *length, size_t *capacity,
                        size_t memsz, uint8_t *borrowed, size_t n) {
  size_t n2 = 1;
  if (n == 0)
    return 0;
  while (n2 < n)
    n2 <<= 1;
  return vec_reserve_(data, length, capacity, memsz, borrowed, n2);
}

size_t vec_compact_(uint8_t **data, size_t *length, size_t *capacity,
                    size_t memsz, uint8_t *borrowed) {
  if (*length == 0) {
    VEC_FREE(*data);
    if (!*borrowed)
      *data = NULL;
    *capacity = 0;
    return 0;
  } else {
    size_t n = *length;
    if (!*borrowed) {
      void *ptr = VEC_ALLOC(*data, n * memsz);
      if (ptr == NULL)
        return -1;
      *data = ptr;
    }
    *capacity = n;
  }
  return 0;
}

size_t vec_insert_(uint8_t **data, size_t *length, size_t *capacity,
                   size_t memsz, uint8_t *borrowed, size_t idx, size_t count) {
  size_t err = vec_expand_(data, length, capacity, memsz * count, borrowed);
  if (err)
    return err;
  memmove(*data + (idx + count) * memsz, *data + idx * memsz,
          (*length - idx) * memsz);
  return 0;
}

void vec_splice_(uint8_t **data, size_t *length, size_t *capacity, size_t memsz,
                 uint8_t *borrowed, size_t start, size_t count) {
  (void)capacity;
  (void)borrowed;
  memmove(*data + start * memsz, *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}

void vec_swapsplice_(uint8_t **data, size_t *length, size_t *capacity,
                     size_t memsz, uint8_t *borrowed, size_t start,
                     size_t count) {
  (void)capacity;
  (void)borrowed;
  memmove(*data + start * memsz, *data + (*length - count) * memsz,
          count * memsz);
}

void vec_swap_(uint8_t **data, size_t *length, size_t *capacity, size_t memsz,
               uint8_t *borrowed, size_t idx1, size_t idx2) {
  size_t count;
  (void)length;
  (void)capacity;
  (void)borrowed;
  if (idx1 == idx2)
    return;
  uint8_t *a = (uint8_t *)*data + idx1 * memsz;
  uint8_t *b = (uint8_t *)*data + idx2 * memsz;
  count = memsz;
  uint8_t tmp;
  while (count--) {
    tmp = *a;
    *a = *b;
    *b = tmp;
    a++, b++;
  }
}
#endif
