#include "hash-table-base.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>

struct list_entry {
    const char *key;
    uint32_t value;
    SLIST_ENTRY(list_entry) pointers;
};

SLIST_HEAD(list_head, list_entry);

struct hash_table_entry {
    struct list_head list_head;
};

struct hash_table_v2 {
    struct hash_table_entry entries[HASH_TABLE_CAPACITY];
    pthread_mutex_t bucket_mutexes[HASH_TABLE_CAPACITY]; // One mutex per bucket
};

struct hash_table_v2 *hash_table_v2_create()
{
    struct hash_table_v2 *hash_table = calloc(1, sizeof(struct hash_table_v2));
    assert(hash_table != NULL);
    for (size_t i = 0; i < HASH_TABLE_CAPACITY; ++i) {
        struct hash_table_entry *entry = &hash_table->entries[i];
        SLIST_INIT(&entry->list_head);
        pthread_mutex_init(&hash_table->bucket_mutexes[i], NULL); // Initialize each bucket's mutex
    }
    return hash_table;
}

static struct hash_table_entry *get_hash_table_entry(struct hash_table_v2 *hash_table,
                                                     const char *key, uint32_t *index)
{
    assert(key != NULL);
    *index = bernstein_hash(key) % HASH_TABLE_CAPACITY;
    struct hash_table_entry *entry = &hash_table->entries[*index];
    return entry;
}

static struct list_entry *get_list_entry(struct list_head *list_head,
                                         const char *key)
{
    assert(key != NULL);

    struct list_entry *entry = NULL;

    SLIST_FOREACH(entry, list_head, pointers) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }
    return NULL;
}

bool hash_table_v2_contains(struct hash_table_v2 *hash_table,
                            const char *key)
{
    uint32_t index;
    struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key, &index);
    pthread_mutex_lock(&hash_table->bucket_mutexes[index]); // Lock the specific bucket
    struct list_head *list_head = &hash_table_entry->list_head;
    struct list_entry *list_entry = get_list_entry(list_head, key);
    bool result = (list_entry != NULL);
    pthread_mutex_unlock(&hash_table->bucket_mutexes[index]); // Unlock the bucket
    return result;
}

void hash_table_v2_add_entry(struct hash_table_v2 *hash_table,
                             const char *key,
                             uint32_t value)
{
    uint32_t index;
    struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key, &index);
    pthread_mutex_lock(&hash_table->bucket_mutexes[index]); // Lock the specific bucket

    struct list_head *list_head = &hash_table_entry->list_head;
    struct list_entry *list_entry = get_list_entry(list_head, key);

    /* Update the value if it already exists */
    if (list_entry != NULL) {
        list_entry->value = value;
        pthread_mutex_unlock(&hash_table->bucket_mutexes[index]); // Unlock the bucket
        return;
    }

    list_entry = calloc(1, sizeof(struct list_entry));
    list_entry->key = key;
    list_entry->value = value;
    SLIST_INSERT_HEAD(list_head, list_entry, pointers);

    pthread_mutex_unlock(&hash_table->bucket_mutexes[index]); // Unlock the bucket
}

uint32_t hash_table_v2_get_value(struct hash_table_v2 *hash_table,
                                 const char *key)
{
    uint32_t index;
    struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key, &index);
    pthread_mutex_lock(&hash_table->bucket_mutexes[index]); // Lock the specific bucket
    struct list_head *list_head = &hash_table_entry->list_head;
    struct list_entry *list_entry = get_list_entry(list_head, key);
    assert(list_entry != NULL);
    uint32_t value = list_entry->value;
    pthread_mutex_unlock(&hash_table->bucket_mutexes[index]); // Unlock the bucket
    return value;
}

void hash_table_v2_destroy(struct hash_table_v2 *hash_table)
{
    for (size_t i = 0; i < HASH_TABLE_CAPACITY; ++i) {
        struct hash_table_entry *entry = &hash_table->entries[i];
        struct list_head *list_head = &entry->list_head;
        struct list_entry *list_entry = NULL;
        while (!SLIST_EMPTY(list_head)) {
            list_entry = SLIST_FIRST(list_head);
            SLIST_REMOVE_HEAD(list_head, pointers);
            free(list_entry);
        }
        pthread_mutex_destroy(&hash_table->bucket_mutexes[i]); // Destroy each bucket's mutex
    }
    free(hash_table);
}
