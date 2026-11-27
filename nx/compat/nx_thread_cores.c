/* GoW3 -> Switch: spread host threads over the three application cores.
 *
 * libnx's pthread_create starts every thread on the process's default core,
 * so the guest PPU threads, the SPU job threads and the RSX thread all shared
 * core 0: SPU jobs waited ~300 ms in the run queue before running for <50 ms,
 * and the game crawled at ~0.3 fps. Linked with -Wl,--wrap=pthread_create,
 * this gives each new thread an ideal core in round-robin over 0..2 (main
 * stays on 0, the first new thread goes to 1).
 * GOW3_CORES=0 turns it off (diagnostic A/B). */
#include <switch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int __real_pthread_create(pthread_t* th, const pthread_attr_t* attr,
                          void* (*fn)(void*), void* arg);

typedef struct { void* (*fn)(void*); void* arg; int core; int n; } nx_core_tramp;

static void* nx_core_entry(void* p)
{
    nx_core_tramp t = *(nx_core_tramp*)p;
    free(p);
    const u32 before = svcGetCurrentProcessorNumber();
    Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, t.core, 0x7);
    if (t.n < 12)
        fprintf(stderr, "[nx-cores] thread #%d born on core %u -> core %d (rc=0x%X)\n",
                t.n, before, t.core, rc);
    return t.fn(t.arg);
}

int __wrap_pthread_create(pthread_t* th, const pthread_attr_t* attr,
                          void* (*fn)(void*), void* arg)
{
    static int s_enabled = -1;
    static int s_next = 0;
    if (s_enabled < 0) {
        const char* e = getenv("GOW3_CORES");
        s_enabled = !(e && e[0] == '0');
    }
    if (!s_enabled) return __real_pthread_create(th, attr, fn, arg);
    nx_core_tramp* t = (nx_core_tramp*)malloc(sizeof *t);
    if (!t) return __real_pthread_create(th, attr, fn, arg);
    const int n = __atomic_fetch_add(&s_next, 1, __ATOMIC_RELAXED);
    t->fn = fn; t->arg = arg; t->n = n;
    t->core = (n + 1) % 3;
    int rc = __real_pthread_create(th, attr, nx_core_entry, t);
    if (rc != 0) free(t);
    return rc;
}
