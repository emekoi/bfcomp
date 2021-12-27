#define DMT_STACK_TRACE
#include "rxi/dmt.h"

#define VEC_ALLOC(ptr, sz) dmt_realloc(ptr, sz)
#define VEC_FREE(ptr) dmt_free(ptr)

#include "rxi/vec.h"

#define MAP_ALLOC(ptr, sz) dmt_realloc(ptr, sz)
#define MAP_FREE(ptr) dmt_free(ptr)

#include "rxi/map.h"

#include "defer.h"
