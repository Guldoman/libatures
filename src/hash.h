#pragma once

#define HASH_SIZE 3769
#define HASH_RETRIES 10
#define HASH(x) ((((x) >> 1)) % hash->size)


#define build_hash_functions(type)                                             \
typedef struct {                                                               \
  const void *address;                                                         \
  type value;                                                                  \
} HashEntry_##type;                                                            \
typedef struct {                                                               \
  size_t size;                                                                 \
  size_t occupied;                                                             \
  HashEntry_##type *entries;                                                   \
} HashTable_##type;                                                            \
static HashTable_##type *new_##type##_hash(void) {                             \
  HashTable_##type *hash = malloc(sizeof(HashTable_##type));                   \
  if (hash == NULL) return NULL;                                               \
  *hash = (HashTable_##type){                                                  \
    .size = HASH_SIZE,                                                         \
    .occupied = 0,                                                             \
    .entries = calloc(HASH_SIZE, sizeof(HashEntry_##type)),                    \
  };                                                                           \
  if (hash->entries == NULL) {                                                 \
    free(hash);                                                                \
    return NULL;                                                               \
  }                                                                            \
  return hash;                                                                 \
}                                                                              \
static inline bool get_from_##type##_hash(HashTable_##type *hash, const void* address, type *value) { \
  size_t index = HASH((uintptr_t)address);                                     \
  HashEntry_##type *entries = hash->entries;                                   \
  for (size_t retries = 0; entries[index].address != NULL; retries++) {        \
    if (entries[index].address == address) {                                   \
      *value = entries[index].value;                                           \
      return true;                                                             \
    }                                                                          \
    if (retries >= HASH_RETRIES) return false;                                 \
    index = (index + 1) % hash->size;                                          \
  }                                                                            \
  return false;                                                                \
}                                                                              \
static bool resize_##type##_hash(HashTable_##type *hash);                      \
static void set_to_##type##_hash(HashTable_##type *hash, const void* address, type value) { \
  size_t index = HASH((uintptr_t)address);                                     \
  HashEntry_##type *entries = hash->entries;                                   \
  size_t retries;                                                              \
  for (retries = 0; entries[index].address != NULL; retries++){                \
    if (retries >= HASH_RETRIES || hash->occupied > hash->size / 3) {          \
      bool success = resize_##type##_hash(hash);                               \
      if (success) {                                                           \
        set_to_##type##_hash(hash, address, value);                            \
        return;                                                                \
      }                                                                        \
    }                                                                          \
    if (retries >= HASH_RETRIES) return;                                       \
    index = (index + 1) % hash->size;                                          \
  }                                                                            \
  entries[index].address = address;                                            \
  entries[index].value = value;                                                \
  hash->occupied++;                                                            \
  return;                                                                      \
}                                                                              \
static bool resize_##type##_hash(HashTable_##type *hash) {                     \
  size_t sizes[] = {4969, 7699, 9769, 11969, 14207, 17669, 22699, 29669, 34693, 44201}; \
  size_t new_size = 0;                                                         \
  for (size_t i = 0; i < sizeof(sizes)/sizeof(size_t); i++) {                  \
    if (sizes[i] > hash->size) {                                               \
      new_size = sizes[i];                                                     \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (new_size == 0) return false;                                             \
  HashEntry_##type *new_entries = calloc(new_size, sizeof(HashEntry_##type));  \
  if (new_entries == NULL) return false;                                       \
  HashEntry_##type *old_entries = hash->entries;                               \
  size_t old_size = hash->size;                                                \
  hash->size = new_size;                                                       \
  hash->occupied = 0;                                                          \
  hash->entries = new_entries;                                                 \
  for (size_t i = 0; i < old_size; i++) {                                      \
    HashEntry_##type entry = old_entries[i];                                   \
    if (entry.address != NULL) {                                               \
      set_to_##type##_hash(hash, entry.address, entry.value);                  \
    }                                                                          \
  }                                                                            \
  free(old_entries);                                                           \
  return true;                                                                 \
}                                                                              \
static void free_##type##_hash(HashTable_##type *hash) {                       \
  if (hash == NULL) return;                                                    \
  free(hash->entries);                                                         \
  hash->entries = NULL;                                                        \
  hash->size = 0;                                                              \
  hash->occupied = 0;                                                          \
  free(hash);                                                                  \
}
