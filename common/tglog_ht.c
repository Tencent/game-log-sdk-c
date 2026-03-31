/*
 * tglog_ht.c - Simple hash table implementation (C99)
 *
 * Open addressing with linear probing. Single-threaded, no locks.
 * Uses MurmurHash3-based hash function for good distribution.
 */

#include <stdlib.h>
#include <string.h>

#include "tglog_ht.h"

/* ======================================================================
 * Hash function: MurmurHash3 finalizer + FNV-1a for byte strings
 * ====================================================================== */

static uint64_t _tglog_ht_hash_mix64(uint64_t key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key;
}

static uint64_t _tglog_ht_hash_bytes(const void *key, uint16_t len, uint64_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    uint64_t hash = seed ^ 0xcbf29ce484222325ULL;

    for (uint16_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }

    return _tglog_ht_hash_mix64(hash);
}

static uint64_t _tglog_ht_hash_direct(uintptr_t key, uint64_t seed) {
    return _tglog_ht_hash_mix64((uint64_t)key ^ seed);
}

/* ======================================================================
 * Internal helpers
 * ====================================================================== */

static uint64_t _next_power_of_2(uint64_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

static void *_ht_malloc(tglog_ht_t *ht, size_t size) {
    if (ht->allocator.malloc)
        return ht->allocator.malloc(size);
    return malloc(size);
}

static void _ht_free(tglog_ht_t *ht, void *ptr, size_t size) {
    if (ht->allocator.free) {
        ht->allocator.free(ptr, size, 0);
        return;
    }
    (void)size;
    free(ptr);
}

/* Compare keys in an entry against a lookup key. */
static int _keys_match(tglog_ht_t *ht, tglog_ht_entry_t *slot,
    uintptr_t key, uint16_t key_length, uint64_t hash) {
    if (slot->hash != hash)
        return 0;

    if (ht->mode == TGLOG_HT_MODE_DIRECT) {
        return slot->key == key;
    } else {
        /* BYTESTRING mode */
        if (slot->key_length != key_length)
            return 0;
        return memcmp((const void *)slot->key, (const void *)key, key_length) == 0;
    }
}

/* Grow the hash table when load factor is too high. */
static int _ht_grow(tglog_ht_t *ht) {
    uint64_t old_capacity = ht->capacity;
    tglog_ht_entry_t *old_table = ht->table;
    uint64_t new_capacity = old_capacity * 2;
    if (new_capacity < 8) new_capacity = 8;

    size_t new_size = (size_t)(new_capacity * sizeof(tglog_ht_entry_t));
    tglog_ht_entry_t *new_table = (tglog_ht_entry_t *)_ht_malloc(ht, new_size);
    if (!new_table)
        return 0;

    memset(new_table, 0, new_size);

    uint64_t new_mask = new_capacity - 1;

    /* Rehash all occupied entries */
    for (uint64_t i = 0; i < old_capacity; i++) {
        if (old_table[i].state != TGLOG_HT_ENTRY_OCCUPIED)
            continue;

        uint64_t idx = old_table[i].hash & new_mask;
        while (new_table[idx].state == TGLOG_HT_ENTRY_OCCUPIED) {
            idx = (idx + 1) & new_mask;
        }

        new_table[idx] = old_table[i];
    }

    ht->table = new_table;
    ht->capacity = new_capacity;
    ht->tombstones = 0;

    if (old_table) {
        _ht_free(ht, old_table, (size_t)(old_capacity * sizeof(tglog_ht_entry_t)));
    }

    return 1;
}

/* ======================================================================
 * Public API
 * ====================================================================== */

int tglog_ht_init(tglog_ht_t *ht, unsigned int mode, void *hash_cb,
    struct tglog_ht_malloc *allocator, uint64_t initial_size, uint64_t seed) {
    (void)hash_cb;

    memset(ht, 0, sizeof(*ht));
    ht->mode = mode;
    ht->seed = seed;

    if (allocator) {
        ht->allocator.malloc = allocator->malloc;
        ht->allocator.free = allocator->free;
    }

    uint64_t capacity = _next_power_of_2(initial_size);
    if (capacity < 8) capacity = 8;

    size_t table_size = (size_t)(capacity * sizeof(tglog_ht_entry_t));
    ht->table = (tglog_ht_entry_t *)_ht_malloc(ht, table_size);
    if (!ht->table)
        return 0;

    memset(ht->table, 0, table_size);
    ht->capacity = capacity;
    ht->count = 0;
    ht->tombstones = 0;

    return 1;
}

void tglog_ht_destroy(tglog_ht_t *ht) {
    if (ht->table) {
        _ht_free(ht, ht->table,
                 (size_t)(ht->capacity * sizeof(tglog_ht_entry_t)));
        ht->table = NULL;
    }
    ht->count = 0;
    ht->capacity = 0;
}

void tglog_ht_hash(tglog_ht_hash_t *h, tglog_ht_t *ht,
    const void *key, uint16_t key_length) {
    if (ht->mode == TGLOG_HT_MODE_DIRECT) {
        h->value = _tglog_ht_hash_direct((uintptr_t)key, ht->seed);
    } else {
        h->value = _tglog_ht_hash_bytes(key, key_length, ht->seed);
    }
}

int tglog_ht_get(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry) {
    uint64_t mask = ht->capacity - 1;
    uint64_t idx = h.value & mask;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        tglog_ht_entry_t *slot = &ht->table[idx];

        if (slot->state == TGLOG_HT_ENTRY_EMPTY)
            return 0;

        if (slot->state == TGLOG_HT_ENTRY_OCCUPIED &&
            _keys_match(ht, slot, entry->key, entry->key_length, h.value)) {
            /* Found: update the entry with the stored values */
            entry->key = slot->key;
            entry->value = slot->value;
            entry->key_length = slot->key_length;
            entry->hash = slot->hash;
            return 1;
        }

        idx = (idx + 1) & mask;
    }

    return 0;
}

int tglog_ht_put(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry) {
    /* Check load factor: grow if > 75% (count + tombstones) */
    if ((ht->count + ht->tombstones) * 4 >= ht->capacity * 3) {
        if (!_ht_grow(ht))
            return 0;
    }

    uint64_t mask = ht->capacity - 1;
    uint64_t idx = h.value & mask;
    int64_t first_tombstone = -1;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        tglog_ht_entry_t *slot = &ht->table[idx];

        if (slot->state == TGLOG_HT_ENTRY_EMPTY) {
            /* Empty slot found, insert here (or at first tombstone) */
            if (first_tombstone >= 0) {
                slot = &ht->table[first_tombstone];
                ht->tombstones--;
            }
            slot->key = entry->key;
            slot->value = entry->value;
            slot->key_length = entry->key_length;
            slot->hash = h.value;
            slot->state = TGLOG_HT_ENTRY_OCCUPIED;
            ht->count++;
            return 1;
        }

        if (slot->state == TGLOG_HT_ENTRY_TOMBSTONE) {
            if (first_tombstone < 0)
                first_tombstone = (int64_t)idx;
        } else if (_keys_match(ht, slot, entry->key, entry->key_length, h.value)) {
            /* Key already exists, put fails */
            entry->key = slot->key;
            entry->value = slot->value;
            entry->key_length = slot->key_length;
            entry->hash = slot->hash;
            return 0;
        }

        idx = (idx + 1) & mask;
    }

    /* Table is full of tombstones, grow and retry */
    if (!_ht_grow(ht))
        return 0;
    return tglog_ht_put(ht, h, entry);
}

int tglog_ht_set(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry) {
    /* Check load factor */
    if ((ht->count + ht->tombstones) * 4 >= ht->capacity * 3) {
        if (!_ht_grow(ht))
            return 0;
    }

    uint64_t mask = ht->capacity - 1;
    uint64_t idx = h.value & mask;
    int64_t first_tombstone = -1;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        tglog_ht_entry_t *slot = &ht->table[idx];

        if (slot->state == TGLOG_HT_ENTRY_EMPTY) {
            /* Insert new entry */
            if (first_tombstone >= 0) {
                slot = &ht->table[first_tombstone];
                ht->tombstones--;
            }
            slot->key = entry->key;
            slot->value = entry->value;
            slot->key_length = entry->key_length;
            slot->hash = h.value;
            slot->state = TGLOG_HT_ENTRY_OCCUPIED;
            ht->count++;
            return 1;
        }

        if (slot->state == TGLOG_HT_ENTRY_TOMBSTONE) {
            if (first_tombstone < 0)
                first_tombstone = (int64_t)idx;
        } else if (_keys_match(ht, slot, entry->key, entry->key_length, h.value)) {
            /* Key exists, update value (and key pointer for BYTESTRING mode) */
            slot->key = entry->key;
            slot->value = entry->value;
            slot->key_length = entry->key_length;
            return 1;
        }

        idx = (idx + 1) & mask;
    }

    /* Table is full of tombstones, grow and retry */
    if (!_ht_grow(ht))
        return 0;
    return tglog_ht_set(ht, h, entry);
}

int tglog_ht_remove(tglog_ht_t *ht, tglog_ht_hash_t h, tglog_ht_entry_t *entry) {
    uint64_t mask = ht->capacity - 1;
    uint64_t idx = h.value & mask;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        tglog_ht_entry_t *slot = &ht->table[idx];

        if (slot->state == TGLOG_HT_ENTRY_EMPTY)
            return 0;

        if (slot->state == TGLOG_HT_ENTRY_OCCUPIED &&
            _keys_match(ht, slot, entry->key, entry->key_length, h.value)) {
            /* Found: copy out the stored key/value before removing */
            entry->key = slot->key;
            entry->value = slot->value;
            entry->key_length = slot->key_length;
            entry->hash = slot->hash;

            slot->state = TGLOG_HT_ENTRY_TOMBSTONE;
            ht->count--;
            ht->tombstones++;
            return 1;
        }

        idx = (idx + 1) & mask;
    }

    return 0;
}

int tglog_ht_next(tglog_ht_t *ht, tglog_ht_iterator_t *iterator,
    tglog_ht_entry_t **entry) {
    while (iterator->offset < ht->capacity) {
        tglog_ht_entry_t *slot = &ht->table[iterator->offset];
        iterator->offset++;

        if (slot->state == TGLOG_HT_ENTRY_OCCUPIED) {
            iterator->current = slot;
            *entry = slot;
            return 1;
        }
    }

    return 0;
}

uint64_t tglog_ht_count(tglog_ht_t *ht) {
    return ht->count;
}
