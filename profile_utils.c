#ifdef CONFIG_QUICKJS_HEAPDUMP
#include <stdint.h>
#include <string.h>
#include "profile_utils.h"

/* -- Allocator ----------------------------------- */
static ProfileAllocator *profile_allocator = NULL;

void profile_set_allocator(ProfileAllocator *alloc) { profile_allocator = alloc; }

void *profile_malloc(size_t size) {
    return profile_allocator->profile_malloc(profile_allocator->opaque, size);
}

void profile_free(void *ptr) { profile_allocator->profile_free(profile_allocator->opaque, ptr); }

void *profile_realloc(void *ptr, size_t size) {
    return profile_allocator->profile_realloc(profile_allocator->opaque, ptr, size);
}

void *profile_mallocz(size_t size) {
    void *ptr = profile_malloc(size);
    if (!ptr)
        return NULL;

    memset(ptr, 0, size);
    return ptr;
}

/* -- String ----------------------------------- */
ProfileString profile_string_new(const char *data, size_t len) {
    char *d = NULL;
    if (len)
        d = profile_malloc(len);

    memcpy(d, data, len);
    return (ProfileString){d, len};
}

void profile_string_free(ProfileString str) { profile_free(str.data); }

/* -- Array ----------------------------------- */
static int profile_array_grow(ProfileArray *arr) {
    if (arr->slots == NULL) {
        arr->slots = profile_mallocz(arr->slot_size * arr->cap);
    } else if (arr->len >= arr->cap) {
        if (arr->cap < 1024) {
            arr->cap += arr->cap;
        } else {
            arr->cap += arr->cap / 4;
        }
        arr->slots = profile_realloc(arr->slots, arr->slot_size * arr->cap);
    }
    return !arr->slots;
}

int profile_array_init(ProfileArray *arr, size_t slot_size, size_t cap) {
    if (!cap)
        cap = 8;
    arr->slot_size = slot_size;
    arr->cap = cap;
    arr->len = 0;
    arr->slots = NULL;
    return profile_array_grow(arr);
}

void profile_array_free(ProfileArray *arr) { profile_free(arr->slots); }

int profile_array_push(ProfileArray *arr, void *item) {
    if (profile_array_grow(arr))
        return -1;
    int i = arr->len++;
    memcpy(arr->slots + (i * arr->slot_size), item, arr->slot_size);
    return i;
}

/* -- Hashmap ----------------------------------- */
unsigned int elf_Hash(const unsigned char *data, unsigned int size) {
    unsigned int hash = 0;
    unsigned int x = 0;
    unsigned int i = 0;

    for (i = 0; i < size; ++data, ++i) {
        hash = (hash << 4) + (*data);

        if ((x = hash & 0xF0000000L) != 0) {
            hash ^= (x >> 24);
        }

        hash &= ~x;
    }

    return hash;
}

void profile_hashmap_init(ProfileHashmap *map, ProfileHashmapKeyCopyFunc *key_copy,
                          ProfileHashmapKeyFreeFunc *key_free,
                          ProfileHashmapValueFreeFunc *value_free) {
    profile_list_init_head(&map->keys);

    map->buckets = profile_mallocz(sizeof(*map->buckets) * PROFILE_HASHMAP_BUCKETS_LEN);
    if (!map->buckets) {
        profile_free(map);
        return;
    }

    for (int i = 0; i < PROFILE_HASHMAP_BUCKETS_LEN; i++) {
        profile_list_init_head(map->buckets + i);
    }

    map->key_copy = key_copy;
    map->key_free = key_free;
    map->value_free = value_free;
    return;
}

ProfileHashkey *profile_hashmap_key_copy(ProfileHashkey *key) {
    ProfileHashkey *k = profile_malloc(sizeof(*k));
    if (!k)
        return NULL;

    profile_list_init_head(&k->link);
    k->hash = -1;

    k->size = key->size;
    k->opaque = profile_malloc(key->size);
    if (!k->opaque) {
        profile_free(k);
        return NULL;
    }
    memcpy(k->opaque, key->opaque, key->size);
    return k;
}

void profile_hashmap_key_free(ProfileHashkey *key) {
    profile_free(key->opaque);
    profile_free(key);
}

ProfileHashkey *profile_hashmap_key_shallow_copy(ProfileHashkey *key) {
    ProfileHashkey *k = profile_malloc(sizeof(*k));
    if (!k)
        return NULL;

    profile_list_init_head(&k->link);
    k->hash = -1;

    k->size = key->size;
    k->opaque = key->opaque;
    return k;
}

void profile_hashmap_key_shallow_free(ProfileHashkey *key) { profile_free(key); }

unsigned int profile_hashmap_hash(ProfileHashkey *key) {
    if (key->hash & 0x80000000) {
        key->hash = elf_Hash(key->opaque, key->size) & PROFILE_HASHMAP_BUCKETS_MASK;
    }
    return key->hash;
}

ProfileHashmapEntry *profile_hashmap_get(ProfileHashmap *map, ProfileHashkey *key) {
    unsigned int hash = profile_hashmap_hash(key);
    ProfileListHead *bucket = map->buckets + hash;

    if (profile_list_empty(bucket))
        return NULL;

    ProfileListHead *el;
    ProfileHashmapEntry *e;
    profile_list_for_each(el, bucket) {
        e = profile_list_entry(el, ProfileHashmapEntry, link);
        if (e->key->size != key->size)
            continue;
        if (memcmp(e->key->opaque, key->opaque, key->size))
            continue;
        return e;
    }

    return NULL;
}

// TODO: resize buckets
int profile_hashmap_set(ProfileHashmap *map, ProfileHashkey *key, void *value,
                        bool free_old) {
    ProfileHashmapEntry *old = profile_hashmap_get(map, key);
    if (old) {
        if (free_old && map->value_free) {
            map->value_free(old->value);
        }
        old->value = value;
        return 0;
    }

    ProfileHashkey *keyp = map->key_copy(key);
    if (!keyp)
        return -1;

    profile_list_add_tail(&keyp->link, &map->keys);

    ProfileHashmapEntry *e = profile_mallocz(sizeof(*e));
    if (!e) {
        map->key_free(keyp);
        return -1;
    }

    profile_list_init_head(&e->link);
    e->key = keyp;
    e->value = value;

    profile_hashmap_hash(keyp);
    ProfileListHead *bucket = map->buckets + keyp->hash;
    profile_list_add(&e->link, bucket);
    return 0;
}

void profile_hashmap_del(ProfileHashmap *map, ProfileHashkey *key) {
    ProfileHashmapEntry *e = profile_hashmap_get(map, key);
    if (!e)
        return;

    profile_list_del(&e->link);

    profile_list_del(&e->key->link);
    if (map->key_free)
        map->key_free(e->key);

    if (map->value_free) {
        map->value_free(e->value);
    }
}

void profile_hashmap_free(ProfileHashmap *map) {
    struct ProfileListHead *el, *el1;
    if (map->key_free) {
        profile_list_for_each(el, &map->keys) {
            ProfileHashkey *k = profile_list_entry(el, ProfileHashkey, link);
            map->key_free(k);
        }
    }

    for (int i = 0; i < PROFILE_HASHMAP_BUCKETS_LEN; i++) {
        ProfileListHead *bucket = (ProfileListHead *)map->buckets + i;
        profile_list_for_each_safe(el, el1, bucket) {
            ProfileHashmapEntry *e = profile_list_entry(el, ProfileHashmapEntry, link);
            if (map->value_free)
                map->value_free(e->value);
            profile_list_del(&e->link);
            profile_free(e);
        }
    }

    profile_free(map->buckets);
}
#endif
