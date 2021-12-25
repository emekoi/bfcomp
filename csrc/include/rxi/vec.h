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

#define vec_unpack_(v)                                                         \
  (uint8_t **)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)

#define vec_t(T)                                                               \
  struct {                                                                     \
    uint64_t length, capacity;                                                 \
    T *data;                                                                   \
  }

#define vec_init(v) memset((v), 0, sizeof(*(v)))

#define vec_deinit(v) (VEC_FREE((v)->data), vec_init(v))

#define vec_push(v, val, ...)                                                  \
  (vec_expand_(vec_unpack_(v)) ? -1 : ((v)->data[(v)->length++] = (val), 0))

#define vec_pop(v) (v)->data[--(v)->length]

#define vec_splice(v, start, count)                                            \
  (vec_splice_(vec_unpack_(v), start, count), (v)->length -= (count))

#define vec_swapsplice(v, start, count)                                        \
  (vec_swapsplice_(vec_unpack_(v), start, count), (v)->length -= (count))

#define vec_insert(v, idx, val, ...)                                           \
  (vec_insert_(vec_unpack_(v), idx)                                            \
       ? -1                                                                    \
       : ((v)->data[idx] = (val), (v)->length++, 0))

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
    int i__, n__ = (count);                                                    \
    if (vec_reserve_po2_(vec_unpack_(v), (v)->length + n__) != 0)              \
      break;                                                                   \
    for (i__ = 0; i__ < n__; i__++) {                                          \
      (v)->data[(v)->length++] = (arr)[i__];                                   \
    }                                                                          \
  } while (0)

#define vec_extend(v, v2) vec_pusharr((v), (v2)->data, (v2)->length)

#define vec_find(v, val, idx)                                                  \
  do {                                                                         \
    for ((idx) = 0; (idx) < (v)->length; (idx)++) {                            \
      if ((v)->data[(idx)] == (val))                                           \
        break;                                                                 \
    }                                                                          \
    if ((idx) == (v)->length)                                                  \
      (idx) = -1;                                                              \
  } while (0)

#define vec_bsearch(v, key, idx, fn)                                           \
  do {                                                                         \
    void *ptr =                                                                \
        bsearch((key), (v)->data, (v)->length, sizeof(*(v)->data), fn);        \
    (idx) = ptr ? (ptr - (void *)(v)->data) / sizeof(*(v)->data) : -1;         \
  } while (0)

#define vec_remove(v, val)                                                     \
  do {                                                                         \
    int idx__;                                                                 \
    vec_find(v, val, idx__);                                                   \
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
           uint64_t i;                                                         \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {0, {0}};                                                    \
         iter.i < (v)->length && ((iter.var = (v)->data[iter.i]), 1);          \
         ++iter.i)

#define vec_for_ptr(v, var, i)                                                 \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           uint64_t i;                                                         \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {0, {0}};                                                    \
         iter.i < (v)->length && ((iter.var = &(v)->data[iter.i]), 1);         \
         ++iter.i)

#define vec_for_rev(v, var, i)                                                 \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           uint64_t i;                                                         \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {(v)->length - 1, {0}};                                      \
         iter.i >= 0 && ((iter.var = (v)->data[iter.i]), 1); --iter.i)

#define vec_for_rev_ptr(v, var, i)                                             \
  if ((v)->length > 0)                                                         \
    for (struct {                                                              \
           uint64_t i;                                                         \
           __typeof__((v)->data[0]) var;                                       \
         } iter = {(v)->length - 1, {0}};                                      \
         iter.i >= 0 && ((iter.var = &(v)->data[iter.i]), 1); --iter.i)

uint64_t vec_expand_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz);
uint64_t vec_reserve_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                      uint64_t memsz, uint64_t n);
uint64_t vec_reserve_po2_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                          uint64_t memsz, uint64_t n);
uint64_t vec_compact_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                      uint64_t memsz);
uint64_t vec_insert_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz, uint64_t idx);
void vec_splice_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                 uint64_t memsz, uint64_t start, uint64_t count);
void vec_swapsplice_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz, uint64_t start, uint64_t count);
void vec_swap_(uint8_t **data, uint64_t *length, uint64_t *capacity,
               uint64_t memsz, uint64_t idx1, uint64_t idx2);
#endif

#ifdef VEC_IMPL
uint64_t vec_expand_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz) {
  if (*length + 1 > *capacity) {
    void *ptr;
    uint64_t n = (*capacity == 0) ? 1 : *capacity << 1;
    ptr = VEC_ALLOC(*data, n * memsz);
    if (ptr == NULL)
      return -1;
    *data = ptr;
    *capacity = n;
  }
  return 0;
}

uint64_t vec_reserve_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                      uint64_t memsz, uint64_t n) {
  (void)length;
  if (n > *capacity) {
    void *ptr = VEC_ALLOC(*data, n * memsz);
    if (ptr == NULL)
      return -1;
    *data = ptr;
    *capacity = n;
  }
  return 0;
}

uint64_t vec_reserve_po2_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                          uint64_t memsz, uint64_t n) {
  uint64_t n2 = 1;
  if (n == 0)
    return 0;
  while (n2 < n)
    n2 <<= 1;
  return vec_reserve_(data, length, capacity, memsz, n2);
}

uint64_t vec_compact_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                      uint64_t memsz) {
  if (*length == 0) {
    VEC_FREE(*data);
    *data = NULL;
    *capacity = 0;
    return 0;
  } else {
    void *ptr;
    uint64_t n = *length;
    ptr = VEC_ALLOC(*data, n * memsz);
    if (ptr == NULL)
      return -1;
    *capacity = n;
    *data = ptr;
  }
  return 0;
}

uint64_t vec_insert_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz, uint64_t idx) {
  uint64_t err = vec_expand_(data, length, capacity, memsz);
  if (err)
    return err;
  memmove(*data + (idx + 1) * memsz, *data + idx * memsz,
          (*length - idx) * memsz);
  return 0;
}

void vec_splice_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                 uint64_t memsz, uint64_t start, uint64_t count) {
  (void)capacity;
  memmove(*data + start * memsz, *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}

void vec_swapsplice_(uint8_t **data, uint64_t *length, uint64_t *capacity,
                     uint64_t memsz, uint64_t start, uint64_t count) {
  (void)capacity;
  memmove(*data + start * memsz, *data + (*length - count) * memsz,
          count * memsz);
}

void vec_swap_(uint8_t **data, uint64_t *length, uint64_t *capacity,
               uint64_t memsz, uint64_t idx1, uint64_t idx2) {
  uint8_t *a, *b, tmp;
  uint64_t count;
  (void)length;
  (void)capacity;
  if (idx1 == idx2)
    return;
  a = (uint8_t *)*data + idx1 * memsz;
  b = (uint8_t *)*data + idx2 * memsz;
  count = memsz;
  while (count--) {
    tmp = *a;
    *a = *b;
    *b = tmp;
    a++, b++;
  }
}
#endif
