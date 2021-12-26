/**
 * Copyright (c) 2014 rxi
 * Copyright (c) 2021 emekoi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <string.h>

#define MAP_VERSION "0.2.0"

#ifndef MAP_ALLOC
#define MAP_ALLOC(ptr, sz) realloc(ptr, sz)
#endif

#ifndef MAP_FREE
#define MAP_FREE(ptr) free(ptr)
#endif

struct map_node_t;
typedef struct map_node_t map_node_t;

typedef struct {
  map_node_t **buckets;
  uint64_t nbuckets, nnodes;
} map_base_t;

typedef struct {
  uint64_t bucketidx;
  map_node_t *node;
} map_iter_t;

#define map_t(T)                                                               \
  struct {                                                                     \
    map_base_t base;                                                           \
    uint64_t count;                                                            \
    T *ref;                                                                    \
    T tmp;                                                                     \
  }

#define map_init(m) memset(m, 0, sizeof(*(m)))

#define map_deinit(m) map_deinit_(&(m)->base)

#define map_get(m, key) ((m)->ref = map_get_(&(m)->base, key, strlen(key) + 1))

#define map_get_ex(m, key, ksize) ((m)->ref = map_get_(&(m)->base, key, ksize))

#define map_set(m, key, value)                                                 \
  ((m)->tmp = (value), (m)->count++,                                           \
   map_set_(&(m)->base, key, strlen(key) + 1, &(m)->tmp, sizeof((m)->tmp)))

#define map_set_ex(m, key, ksize, value)                                       \
  ((m)->tmp = (value), (m)->count++,                                           \
   map_set_(&(m)->base, key, ksize, &(m)->tmp, sizeof((m)->tmp)))

#define map_remove(m, key)                                                     \
  ((m)->count--, map_remove_(&(m)->base, key, strlen(key) + 1))

#define map_remove_ex(m, key, ksize)                                           \
  ((m)->count--, map_remove_(&(m)->base, key, ksize))

#define map_iter(m) map_iter_()

#define map_next(m, iter) map_next_(&(m)->base, iter)

void map_deinit_(map_base_t *m);
void *map_get_(map_base_t *m, const void *key, uint64_t ksize);
uint64_t map_set_(map_base_t *m, const void *key, uint64_t ksize, void *value,
                  uint64_t vsize);
void map_remove_(map_base_t *m, const void *key, uint64_t ksize);
map_iter_t map_iter_(void);
const char *map_next_(map_base_t *m, map_iter_t *iter);

#endif

#ifdef MAP_IMPL
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct map_node_t {
  uint64_t hash;
  void *value;
  map_node_t *next;
};

static uint64_t map_hash(const void *str, uint64_t ksize) {
  uint64_t hash = 5381;
  while (ksize--) {
    hash = ((hash << 5) + hash) ^ *(uint8_t *)str++;
  }
  return hash;
}

static map_node_t *map_newnode(const void *key, uint64_t ksize, void *value,
                               uint64_t vsize) {
  map_node_t *node;
  uint64_t voffset = ksize + ((sizeof(void *) - ksize) % sizeof(void *));
  node = MAP_ALLOC(NULL, sizeof(*node) + voffset + vsize);
  if (!node)
    return NULL;
  memcpy(node + 1, key, ksize);
  node->hash = map_hash(key, ksize);
  node->value = ((char *)(node + 1)) + voffset;
  memcpy(node->value, value, vsize);
  return node;
}

static uint64_t map_bucketidx(map_base_t *m, uint64_t hash) {
  /* If the implementation is changed to allow a non-power-of-2 bucket count,
   * the line below should be changed to use mod instead of AND */
  return hash & (m->nbuckets - 1);
}

static void map_addnode(map_base_t *m, map_node_t *node) {
  uint64_t n = map_bucketidx(m, node->hash);
  node->next = m->buckets[n];
  m->buckets[n] = node;
}

static uint64_t map_resize(map_base_t *m, uint64_t nbuckets) {
  map_node_t *nodes, *node, *next;
  map_node_t **buckets;
  uint64_t i;
  /* Chain all nodes together */
  nodes = NULL;
  i = m->nbuckets;
  while (i--) {
    node = (m->buckets)[i];
    while (node) {
      next = node->next;
      node->next = nodes;
      nodes = node;
      node = next;
    }
  }
  /* Reset buckets */
  buckets = MAP_ALLOC(m->buckets, sizeof(*m->buckets) * nbuckets);
  if (buckets != NULL) {
    m->buckets = buckets;
    m->nbuckets = nbuckets;
  }
  if (m->buckets) {
    memset(m->buckets, 0, sizeof(*m->buckets) * m->nbuckets);
    /* Re-add nodes to buckets */
    node = nodes;
    while (node) {
      next = node->next;
      map_addnode(m, node);
      node = next;
    }
  }
  /* Return error code if MAP_ALLOC failed */
  return (buckets == NULL) ? -1 : 0;
}

static map_node_t **map_getref(map_base_t *m, const void *key, uint64_t ksize) {
  uint64_t hash = map_hash(key, ksize);
  map_node_t **next;
  if (m->nbuckets > 0) {
    next = &m->buckets[map_bucketidx(m, hash)];
    while (*next) {
      if ((*next)->hash == hash && !strcmp((char *)(*next + 1), key)) {
        return next;
      }
      next = &(*next)->next;
    }
  }
  return NULL;
}

void map_deinit_(map_base_t *m) {
  map_node_t *next, *node;
  uint64_t i;
  i = m->nbuckets;
  while (i--) {
    node = m->buckets[i];
    while (node) {
      next = node->next;
      MAP_FREE(node);
      node = next;
    }
  }
  MAP_FREE(m->buckets);
}

void *map_get_(map_base_t *m, const void *key, uint64_t ksize) {
  map_node_t **next = map_getref(m, key, ksize);
  return next ? (*next)->value : NULL;
}

uint64_t map_set_(map_base_t *m, const void *key, uint64_t ksize, void *value,
                  uint64_t vsize) {
  uint64_t n, err;
  map_node_t **next, *node;
  /* Find & replace existing node */
  next = map_getref(m, key, ksize);
  if (next) {
    memcpy((*next)->value, value, vsize);
    return 0;
  }
  /* Add new node */
  node = map_newnode(key, ksize, value, vsize);
  if (node == NULL)
    goto fail;
  if (m->nnodes >= m->nbuckets) {
    n = (m->nbuckets > 0) ? (m->nbuckets << 1) : 1;
    err = map_resize(m, n);
    if (err)
      goto fail;
  }
  map_addnode(m, node);
  m->nnodes++;
  return 0;
fail:
  if (node)
    MAP_FREE(node);
  return -1;
}

void map_remove_(map_base_t *m, const void *key, uint64_t ksize) {
  map_node_t *node;
  map_node_t **next = map_getref(m, key, ksize);
  if (next) {
    node = *next;
    *next = (*next)->next;
    MAP_FREE(node);
    m->nnodes--;
  }
}

map_iter_t map_iter_(void) {
  map_iter_t iter;
  iter.bucketidx = -1;
  iter.node = NULL;
  return iter;
}

const char *map_next_(map_base_t *m, map_iter_t *iter) {
  if (iter->node) {
    iter->node = iter->node->next;
    if (iter->node == NULL)
      goto nextBucket;
  } else {
  nextBucket:
    do {
      if (++iter->bucketidx >= m->nbuckets) {
        return NULL;
      }
      iter->node = m->buckets[iter->bucketidx];
    } while (iter->node == NULL);
  }
  return (char *)(iter->node + 1);
}
#endif
