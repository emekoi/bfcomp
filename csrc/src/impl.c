#define DMT_IMPL

#define VEC_ALLOC(ptr, sz) dmt_realloc(ptr, sz)
#define VEC_FREE(ptr) dmt_free(ptr)

#define VEC_IMPL

#define MAP_ALLOC(ptr, sz) dmt_realloc(ptr, sz)
#define MAP_FREE(ptr) dmt_free(ptr)

#define MAP_IMPL

#define DEFER_IMPL

#include "common.h"
