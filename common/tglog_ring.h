/*
 * tglog_ring.h - Cross-platform ring buffer with a C99-compatible API
 *
 * This header is header-only and keeps the public interface usable from C99
 * code, but its concurrent semantics rely on tglog_atomic.h, which is backed
 * by compiler-specific atomic primitives rather than any C99 standard atomic
 * facility.
 *
 * Supports SPSC (single producer, single consumer) and
 * MPSC (multi-producer, single consumer) modes.
 *
 * The buffer uses a caller-provided array of typed elements.
 * Use TGLOG_RING_PROTOTYPE(name, type) to generate type-safe functions.
 */

#ifndef TGLOG_RING_H
#define TGLOG_RING_H

#include <string.h>
#include <assert.h>

#include "tglog_atomic.h"

/*
 * Cache line size for padding to avoid false sharing.
 */
#ifndef TGLOG_CACHELINE
#define TGLOG_CACHELINE 64
#endif

/*
 * Compiler hints.
 */
#if defined(__GNUC__) || defined(__clang__)
#define TGLOG_CC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define TGLOG_CC_LIKELY(x)   __builtin_expect(!!(x), 1)
#else
#define TGLOG_CC_UNLIKELY(x) (x)
#define TGLOG_CC_LIKELY(x)   (x)
#endif

/*
 * Concurrent ring buffer structure.
 *
 * Each counter lives on its own cache line to minimise false sharing:
 *   - c_head is written by the consumer, read by producers (full-check)
 *   - p_head is written by producers (CAS in MP enqueue), read by consumer
 *           in dequeue only indirectly via p_tail
 *   - p_tail is written by producers at commit, read by the consumer on
 *           every dequeue and by producers to serialise in-order commit
 *
 * If p_head and p_tail share a line, every CAS on p_head invalidates the
 * consumer's cached copy of p_tail (and vice versa) even though p_tail
 * itself has not changed. Three separate lines eliminate that contention.
 *
 * The struct itself is aligned to a cache line so that embedding it in
 * another structure does not misalign the padded layout.
 */
#if defined(_MSC_VER)
__declspec(align(TGLOG_CACHELINE))
#endif
struct tglog_ring {
    unsigned int c_head;
    char pad1[TGLOG_CACHELINE - sizeof(unsigned int)];
    unsigned int p_tail;
    char pad2[TGLOG_CACHELINE - sizeof(unsigned int)];
    unsigned int p_head;
    char pad3[TGLOG_CACHELINE - sizeof(unsigned int)];
    unsigned int size;
    unsigned int mask;
}
#if (defined(__GNUC__) || defined(__clang__))
__attribute__((aligned(TGLOG_CACHELINE)))
#endif
;
typedef struct tglog_ring tglog_ring_t;

/* Ring buffer element wrapper (for pointer-based API) */
struct tglog_ring_buffer {
    void *value;
};
typedef struct tglog_ring_buffer tglog_ring_buffer_t;

/*
 * Initialize a ring buffer.
 * @param ring  The ring buffer.
 * @param size  The capacity (MUST be a power of 2).
 */
static inline void tglog_ring_init(struct tglog_ring *ring, unsigned int size) {
    assert(size >= 2 && (size & (size - 1)) == 0 && "size must be a power of 2");
    ring->size = size;
    ring->mask = size - 1;
    ring->p_tail = 0;
    ring->p_head = 0;
    ring->c_head = 0;
}

/* Get the number of items in the ring buffer. */
static inline unsigned int tglog_ring_size(const struct tglog_ring *ring) {
    unsigned int c, p;

    c = tglog_atomic_load_uint(&ring->c_head);
    p = tglog_atomic_load_uint(&ring->p_tail);
    return (p - c) & ring->mask;
}

/* Get the capacity of the ring buffer. */
static inline unsigned int tglog_ring_capacity(const struct tglog_ring *ring) {
    return ring->size;
}

/* ======================================================================
 * Internal implementations
 * ====================================================================== */

/* Single-producer enqueue (used by SPSC and SPMC). */
static inline int _tglog_ring_enqueue_sp(struct tglog_ring *ring,
    void *buffer, const void *entry, unsigned int ts, unsigned int *size) {
    const unsigned int mask = ring->mask;
    unsigned int consumer, producer, delta;

    consumer = tglog_atomic_load_uint(&ring->c_head);
    producer = ring->p_tail;
    delta = producer + 1;
    if (size != NULL)
        *size = (producer - consumer) & mask;

    if (TGLOG_CC_UNLIKELY((delta & mask) == (consumer & mask)))
        return 0;

    memcpy((char *)buffer + ts * (producer & mask), entry, ts);

    /* Ensure slot data is visible before updating producer counter. */
    tglog_fence_store();
    tglog_atomic_store_uint((unsigned int *)&ring->p_tail, delta);
    return 1;
}

/* Single-consumer dequeue (used by SPSC and MPSC). */
static inline int _tglog_ring_dequeue_sc(struct tglog_ring *ring,
    const void *buffer, void *target, unsigned int size) {
    const unsigned int mask = ring->mask;
    unsigned int consumer, producer;

    consumer = ring->c_head;
    producer = tglog_atomic_load_uint(&ring->p_tail);

    if (TGLOG_CC_UNLIKELY(consumer == producer))
        return 0;

    /* Serialize with respect to our snapshot of the producer counter. */
    tglog_fence_load();

    memcpy(target, (const char *)buffer + size * (consumer & mask), size);

    /* Ensure copy is completed before updating consumer counter. */
    tglog_fence_store();
    tglog_atomic_store_uint((unsigned int *)&ring->c_head, consumer + 1);
    return 1;
}

/*
 * Maximum number of pause-spins before yielding the CPU in MPSC commit loop.
 * If a preceding producer is preempted after reserving a slot but before
 * committing p_tail, all later producers (and the consumer) are blocked.
 * We spin briefly, then yield to give the stalled producer a chance to run.
 */
#ifndef TGLOG_RING_SPIN_LIMIT
#define TGLOG_RING_SPIN_LIMIT 1024
#endif

/* Multi-producer enqueue (used by MPSC and MPMC). */
static inline int _tglog_ring_enqueue_mp(struct tglog_ring *ring,
    void *buffer, const void *entry, unsigned int ts, unsigned int *size) {
    const unsigned int mask = ring->mask;
    unsigned int producer, consumer, delta;

    producer = tglog_atomic_load_uint(&ring->p_head);

    for (;;) {
        /*
         * The snapshot of producer must be up to date with respect to consumer.
         */
        tglog_fence_load();
        consumer = tglog_atomic_load_uint(&ring->c_head);

        delta = producer + 1;

        /* Only try CAS if not clearly full. */
        if (TGLOG_CC_LIKELY((producer - consumer) < mask)) {
            if (tglog_atomic_cas_uint((unsigned int *)&ring->p_head,
                producer, delta)) {
                break;
            }
            /* CAS failed, reload p_head */
            producer = tglog_atomic_load_uint(&ring->p_head);
        } else {
            unsigned int new_producer;

            /* Slow path. Either full or stale p_head snapshot. */
            tglog_fence_load();
            new_producer = tglog_atomic_load_uint(&ring->p_head);

            if (producer == new_producer) {
                /* Truly full */
                if (size != NULL)
                    *size = (producer - consumer) & mask;
                return 0;
            }

            producer = new_producer;
        }
    }

    memcpy((char *)buffer + ts * (producer & mask), entry, ts);

    /*
     * Wait until all previous producers have committed.
     * This ensures p_tail advances in order.
     *
     * Back-off strategy: spin with pause for TGLOG_RING_SPIN_LIMIT iterations,
     * then yield the CPU so a preempted predecessor can make progress.
     */
    {
        unsigned int spin_count = 0;
        while (tglog_atomic_load_uint(&ring->p_tail) != producer) {
            if (TGLOG_CC_LIKELY(spin_count < TGLOG_RING_SPIN_LIMIT)) {
                tglog_pause();
                spin_count++;
            } else {
                tglog_yield();
            }
        }
    }

    /* Ensure data is visible before updating p_tail. */
    tglog_fence_store();
    tglog_atomic_store_uint((unsigned int *)&ring->p_tail, delta);

    if (size != NULL)
        *size = (producer - consumer) & mask;

    return 1;
}

/* ======================================================================
 * Public pointer-based API (SPSC)
 * ====================================================================== */

static inline int tglog_ring_enqueue_spsc(struct tglog_ring *ring,
    struct tglog_ring_buffer *buffer, const void *entry) {
    return _tglog_ring_enqueue_sp(ring, buffer, &entry, sizeof(entry), NULL);
}

static inline int tglog_ring_dequeue_spsc(struct tglog_ring *ring,
    const struct tglog_ring_buffer *buffer, void *data) {
    return _tglog_ring_dequeue_sc(ring, buffer, (void **)data, sizeof(void *));
}

/* ======================================================================
 * Public pointer-based API (MPSC)
 * ====================================================================== */

static inline int tglog_ring_enqueue_mpsc(struct tglog_ring *ring,
    struct tglog_ring_buffer *buffer, const void *entry) {
    return _tglog_ring_enqueue_mp(ring, buffer, &entry, sizeof(entry), NULL);
}

static inline int tglog_ring_dequeue_mpsc(struct tglog_ring *ring,
    const struct tglog_ring_buffer *buffer, void *data) {
    return _tglog_ring_dequeue_sc(ring, buffer, (void **)data, sizeof(void *));
}

/* ======================================================================
 * TGLOG_RING_PROTOTYPE - Generate type-safe ring buffer functions
 *
 * Usage:
 *   TGLOG_RING_PROTOTYPE(msg_queue, tglog_message*)
 *
 * This generates functions like:
 *   tglog_ring_enqueue_spsc_msg_queue(ring, buffer, entry)
 *   tglog_ring_dequeue_spsc_msg_queue(ring, buffer, entry)
 *   tglog_ring_enqueue_mpsc_msg_queue(ring, buffer, entry)
 *   tglog_ring_dequeue_mpsc_msg_queue(ring, buffer, entry)
 * ====================================================================== */

#define TGLOG_RING_PROTOTYPE(name, type)                                    \
                                                                            \
static inline int                                                           \
tglog_ring_enqueue_spsc_##name(struct tglog_ring *a,                        \
    struct type *b,                                                         \
    struct type *c) {                                                       \
    return _tglog_ring_enqueue_sp(a, b, c,                                  \
        sizeof(struct type), NULL);                                         \
}                                                                           \
                                                                            \
static inline int                                                           \
tglog_ring_dequeue_spsc_##name(struct tglog_ring *a,                        \
    struct type *b,                                                         \
    struct type *c) {                                                       \
    return _tglog_ring_dequeue_sc(a, b, c,                                  \
        sizeof(struct type));                                               \
}                                                                           \
                                                                            \
static inline int                                                           \
tglog_ring_enqueue_mpsc_##name(struct tglog_ring *a,                        \
    struct type *b,                                                         \
    struct type *c) {                                                       \
    return _tglog_ring_enqueue_mp(a, b, c,                                  \
        sizeof(struct type), NULL);                                         \
}                                                                           \
                                                                            \
static inline int                                                           \
tglog_ring_dequeue_mpsc_##name(struct tglog_ring *a,                        \
    struct type *b,                                                         \
    struct type *c) {                                                       \
    return _tglog_ring_dequeue_sc(a, b, c,                                  \
        sizeof(struct type));                                               \
}

#endif /* TGLOG_RING_H */
