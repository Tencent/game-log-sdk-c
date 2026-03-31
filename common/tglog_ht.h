/*
 * tglog_ht.h - Simple hash table (C99)
 *
 * Single-threaded hash table with open addressing and linear probing.
 * Supports two modes:
 *   - TGLOG_HT_MODE_DIRECT: keys are integer/pointer values
 *   - TGLOG_HT_MODE_BYTESTRING: keys are byte sequences
 *
 * API is designed to be a drop-in replacement for CK hash table operations.
 */

#ifndef TGLOG_HT_H
#define TGLOG_HT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hash table modes */
#define TGLOG_HT_MODE_DIRECT       1U
#define TGLOG_HT_MODE_BYTESTRING   2U

/* Hash value */
struct tglog_ht_hash {
    uint64_t value;
};
typedef struct tglog_ht_hash tglog_ht_hash_t;

/* Entry states */
#define TGLOG_HT_ENTRY_EMPTY     0
#define TGLOG_HT_ENTRY_OCCUPIED  1
#define TGLOG_HT_ENTRY_TOMBSTONE 2

/*
 * Hash table entry.
 * For BYTESTRING mode: key is a pointer to the key data, key_length is the length.
 * For DIRECT mode: key is the key value itself (e.g., a pointer cast to uintptr_t).
 */
struct tglog_ht_entry {
    uintptr_t key;
    uintptr_t value;
    uint16_t key_length;
    uint64_t hash;
    uint8_t state;
};
typedef struct tglog_ht_entry tglog_ht_entry_t;

/* Custom allocator */
struct tglog_ht_malloc {
    void *(*malloc)(size_t);
    void (*free)(void *, size_t, int);   /* (ptr, size, defer) */
};

/* Hash table */
struct tglog_ht {
    tglog_ht_entry_t *table;
    uint64_t capacity;
    uint64_t count;
    uint64_t tombstones;
    uint32_t mode;
    uint64_t seed;
    struct tglog_ht_malloc allocator;
};
typedef struct tglog_ht tglog_ht_t;

/* Iterator */
struct tglog_ht_iterator {
    tglog_ht_entry_t *current;
    uint64_t offset;
};
typedef struct tglog_ht_iterator tglog_ht_iterator_t;

#define TGLOG_HT_ITERATOR_INITIALIZER { NULL, 0 }

/* ======================================================================
 * Iterator initialization
 * ====================================================================== */

static inline void tglog_ht_iterator_init(struct tglog_ht_iterator *iterator) {
    iterator->current = NULL;
    iterator->offset = 0;
}

/* ======================================================================
 * Entry manipulation (inline, compatible with CK API)
 * ====================================================================== */

/* Set key in entry (for BYTESTRING mode lookups). */
static inline void tglog_ht_entry_key_set(tglog_ht_entry_t *entry,
    const void *key, uint16_t key_length) {
    entry->key = (uintptr_t)key;
    entry->key_length = key_length;
}

/* Set key, value, and hash in entry (for insert/update operations). */
static inline void tglog_ht_entry_set(tglog_ht_entry_t *entry,
    tglog_ht_hash_t h, const void *key, uint16_t key_length,
    const void *value) {
    entry->key = (uintptr_t)key;
    entry->value = (uintptr_t)value;
    entry->key_length = key_length;
    entry->hash = h.value;
}

/* Get key pointer from entry. */
static inline void *tglog_ht_entry_key(tglog_ht_entry_t *entry) {
    return (void *)entry->key;
}

/* Get key length from entry. */
static inline uint16_t tglog_ht_entry_key_length(tglog_ht_entry_t *entry) {
    return entry->key_length;
}

/* Get value from entry. */
static inline void *tglog_ht_entry_value(tglog_ht_entry_t *entry) {
    return (void *)entry->value;
}

/* Check if entry is empty. */
static inline int tglog_ht_entry_empty(tglog_ht_entry_t *entry) {
    return entry->state == TGLOG_HT_ENTRY_EMPTY;
}

/* ======================================================================
 * Hash table operations (declared, implemented in tglog_ht.c)
 * ====================================================================== */

/*
 * Initialize a hash table.
 * @param ht         The hash table.
 * @param mode       TGLOG_HT_MODE_DIRECT or TGLOG_HT_MODE_BYTESTRING.
 * @param hash_cb    Unused, kept for API compatibility (pass NULL).
 * @param allocator  Custom allocator (can be NULL for default malloc/free).
 * @param initial_size  Initial capacity (will be rounded up to power of 2).
 * @param seed       Hash seed.
 * @return 1 on success, 0 on failure.
 */
int tglog_ht_init(tglog_ht_t *ht, unsigned int mode, void *hash_cb,
                   struct tglog_ht_malloc *allocator, uint64_t initial_size,
                   uint64_t seed);

void tglog_ht_destroy(tglog_ht_t *ht);

void tglog_ht_hash(tglog_ht_hash_t *h, tglog_ht_t *ht,
                    const void *key, uint16_t key_length);

/*
 * Get an entry by key. The entry's key/key_length must be set before calling.
 * On success, the entry is updated with the found key/value/hash.
 * @return 1 if found, 0 if not found.
 */
int tglog_ht_get(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry);

/*
 * Insert a new entry. Fails if the key already exists.
 * @return 1 on success, 0 if key already exists or allocation failure.
 */
int tglog_ht_put(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry);

/*
 * Insert or update an entry (upsert).
 * @return 1 on success, 0 on allocation failure.
 */
int tglog_ht_set(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry);

/*
 * Remove an entry by key. The entry's key/key_length must be set before calling.
 * On success, the entry is updated with the removed key/value.
 * @return 1 if removed, 0 if not found.
 */
int tglog_ht_remove(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry);

/*
 * Iterate to the next entry.
 * @return 1 if an entry was found, 0 if iteration is complete.
 */
int tglog_ht_next(tglog_ht_t *ht, tglog_ht_iterator_t *iterator,
                   tglog_ht_entry_t **entry);

/* Get the number of entries. */
uint64_t tglog_ht_count(tglog_ht_t *ht);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_HT_H */
