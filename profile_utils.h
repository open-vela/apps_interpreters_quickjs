#ifndef PROFILE_UTILS_H
#define PROFILE_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* -- Allocator ----------------------------------- */
typedef struct ProfileAllocator ProfileAllocator;
typedef void *ProfileMallocFunc(void *opaque, size_t size);
typedef void ProfileFreeFunc(void *opaque, void *ptr);
typedef void *ProfileReallocFunc(void *opaque, void *ptr, size_t size);
typedef struct ProfileAllocator {
    void *opaque;
    ProfileMallocFunc *profile_malloc;
    ProfileFreeFunc *profile_free;
    ProfileReallocFunc *profile_realloc;
} ProfileAllocator;

void profile_set_allocator(ProfileAllocator *alloc);

void *profile_malloc(size_t size);
void profile_free(void *ptr);
void *profile_realloc(void *ptr, size_t size);
void *profile_mallocz(size_t size);

/* -- String ----------------------------------- */
typedef struct ProfileString {
    char *data;
    size_t len;
} ProfileString;

ProfileString profile_string_new(const char *data, size_t len);
void profile_string_free(ProfileString str);

/* -- Array ----------------------------------- */
typedef struct ProfileArray {
    void *slots;
    size_t slot_size;
    size_t len;
    size_t cap;
} ProfileArray;

int profile_array_init(ProfileArray *arr, size_t slot_size, size_t cap);
void profile_array_free(ProfileArray *arr);
int profile_array_push(ProfileArray *arr, void *item);

#define profile_array_el(arr, typ, i) ((typ *)((arr)->slots) + i)

/* -- List ----------------------------------- */
typedef struct ProfileListHead ProfileListHead;
struct ProfileListHead {
    ProfileListHead *prev;
    ProfileListHead *next;
};

static inline void profile_list_init_head(ProfileListHead *head) {
    head->prev = head;
    head->next = head;
}

static inline int profile_list_empty(ProfileListHead *el) { return el->next == el; }

/* return the pointer of type 'type *' containing 'el' as field 'member' */
#define profile_list_entry(el, type, member)                                       \
  ((type *)((uint8_t *)(el)-offsetof(type, member)))

/* insert 'el' between 'prev' and 'next' */
static inline void __profile_list_add(ProfileListHead *el, struct ProfileListHead *prev,
                                      ProfileListHead *next) {
    prev->next = el;
    el->prev = prev;
    el->next = next;
    next->prev = el;
}

/* add 'el' at the head of the list 'head' (= after element head) */
static inline void profile_list_add(ProfileListHead *el, ProfileListHead *head) {
    __profile_list_add(el, head, head->next);
}

/* add 'el' at the end of the list 'head' (= before element head) */
static inline void profile_list_add_tail(ProfileListHead *el, ProfileListHead *head) {
    __profile_list_add(el, head->prev, head);
}

static inline void profile_list_del(ProfileListHead *el) {
    ProfileListHead *prev, *next;
    prev = el->prev;
    next = el->next;
    prev->next = next;
    next->prev = prev;
    el->prev = NULL; /* fail safe */
    el->next = NULL; /* fail safe */
}

#define profile_list_for_each(el, head)                                            \
  for (el = (head)->next; el != (__typeof__(el))(head); el = el->next)

#define profile_list_for_each_safe(el, el1, head)                                  \
  for (el = (head)->next, el1 = el->next; el != (__typeof__(el))(head);            \
       el = el1, el1 = el->next)

#define profile_list_for_each_prev(el, head)                                       \
  for (el = (head)->prev; el != (__typeof__(el))(head); el = el->prev)

#define profile_list_for_each_prev_safe(el, el1, head)                             \
  for (el = (head)->prev, el1 = el->prev; el != (__typeof__(el))(head);            \
       el = el1, el1 = el->prev)

/* -- Hashmap ----------------------------------- */
typedef struct ProfileHashkey ProfileHashkey;
struct ProfileHashkey {
    ProfileListHead link;
    size_t size;
    void *opaque;
    unsigned int hash;
};

#define PROFILE_HASHMAP_BUCKETS_LEN 64
#define PROFILE_HASHMAP_BUCKETS_MASK (PROFILE_HASHMAP_BUCKETS_LEN - 1)

typedef struct ProfileHashmap ProfileHashmap;
typedef struct ProfileHashmapEntry ProfileHashmapEntry;
typedef void ProfileHashmapValueFreeFunc(void *ptr);
struct ProfileHashmapEntry {
    ProfileListHead link;
    ProfileHashkey *key;
    void *value;
};

typedef ProfileHashkey *ProfileHashmapKeyCopyFunc(ProfileHashkey *key);
typedef void ProfileHashmapKeyFreeFunc(ProfileHashkey *key);
struct ProfileHashmap {
    ProfileListHead keys;     // List<ProfileHashkey*>
    ProfileListHead *buckets; // Array<List<ProfileListHead>>

    ProfileHashmapKeyCopyFunc *key_copy;
    ProfileHashmapKeyFreeFunc *key_free;
    ProfileHashmapValueFreeFunc *value_free;
};

void profile_hashmap_init(ProfileHashmap *map, ProfileHashmapKeyCopyFunc *key_copy,
                          ProfileHashmapKeyFreeFunc *key_free,
                          ProfileHashmapValueFreeFunc *value_free);

ProfileHashkey *profile_hashmap_key_copy(ProfileHashkey *key);
void profile_hashmap_key_free(ProfileHashkey *key);

ProfileHashkey *profile_hashmap_key_shallow_copy(ProfileHashkey *key);
void profile_hashmap_key_shallow_free(ProfileHashkey *key);

int profile_hashmap_set(ProfileHashmap *map, ProfileHashkey *key, void *value,
                        bool free_old);
ProfileHashmapEntry *profile_hashmap_get(ProfileHashmap *map, ProfileHashkey *key);
void profile_hashmap_del(ProfileHashmap *map, ProfileHashkey *key);
void profile_hashmap_free(ProfileHashmap *map);

#endif
