#include <string.h>
#include <stdio.h>

#include "glypharray.h"

// typedef struct GlyphArray {
//   size_t len;
//   size_t allocated;
//   uint16_t *array;
//   Bloom bloom;
//   bool bloom_valid;
// } GlyphArray;

GlyphArray *GlyphArray_new(size_t size) {
  GlyphArray *ga = malloc(sizeof(GlyphArray));
  if (ga == NULL) return NULL;
  ga->len = 0;
  ga->allocated = size;
  ga->bloom = null_bloom;
  ga->bloom_valid = true;
  ga->array = malloc(sizeof(uint16_t) * size);
  if (ga->array == NULL) {
    free(ga);
    return NULL;
  }
  return ga;
}

void GlyphArray_free(GlyphArray *ga) {
  if (ga == NULL) return;
  free(ga->array);
  ga->array = NULL;
  free(ga);
}


bool GlyphArray_set1(GlyphArray *glyph_array, size_t index, uint16_t data) {
  GlyphArray *ga = glyph_array;
  if (index > ga->len) {
    return false;
  }
  ga->bloom_valid = false;
  ga->array[index] = data;
  return true;
}

bool GlyphArray_set(GlyphArray *glyph_array, size_t from, const uint16_t *data, size_t data_size) {
  GlyphArray *ga = glyph_array;
  if (from > ga->len) {
    // TODO: error out maybe?
    return false;
  }
  ga->bloom_valid = false;
  if (from + data_size > ga->len) {
    size_t remainder = (from + data_size) - ga->len;
    if (ga->len + remainder > ga->allocated) {
      size_t new_size = (ga->len + remainder) * 1.3;
      if (new_size < ga->allocated) {
        // Would have overflown
        return false;
      }
      // Need to do overlapped operation, but we're risking reallocation.
      // We could lose the "old" location before we can actually do the operation,
      // so we back it up first.
      if ((data >= ga->array && data < ga->array + ga->len) ||
          (data + data_size > ga->array && data + data_size <= ga->array + ga->len)) {
        uint16_t *new_array = NULL;
        new_array = malloc(sizeof(uint16_t) * new_size);
        if (new_array == NULL) return false;
        memcpy(new_array, ga->array, ga->len * sizeof(uint16_t));
        uint16_t *old_array = ga->array;
        ga->array = new_array;
        ga->allocated = new_size;

        bool res = GlyphArray_set(ga, from, data, data_size);
        free(old_array);
        return res;
      }
      uint16_t *_array = realloc(ga->array, sizeof(uint16_t) * new_size);
      if (_array == NULL) {
        return false;
      }
      ga->array = _array;
      ga->allocated = new_size;
    }
    ga->len += remainder;
  }
  memcpy(&ga->array[from], data, data_size * sizeof(uint16_t));
  return true;
}

bool GlyphArray_put(GlyphArray *dst, size_t dst_index, GlyphArray *src, size_t src_index, size_t len) {
  if (src_index + len > src->len) return false;
  return GlyphArray_set(dst, dst_index, &src->array[src_index], len);
}

bool GlyphArray_shrink(GlyphArray *glyph_array, size_t reduction) {
  GlyphArray *ga = glyph_array;
  if (reduction > ga->len) {
    return false;
  }
  ga->bloom_valid = false;
  ga->len -= reduction;
  return true;
}

bool GlyphArray_append(GlyphArray *glyph_array, const uint16_t *data, size_t data_size) {
  return GlyphArray_set(glyph_array, glyph_array->len, data, data_size);
}

GlyphArray * GlyphArray_new_from_GlyphArray(const GlyphArray *glyph_array) {
  GlyphArray *ga = GlyphArray_new(glyph_array->len);
  if (ga == NULL) return NULL;
  if (!GlyphArray_append(ga, glyph_array->array, glyph_array->len)) {
    GlyphArray_free(ga);
    return NULL;
  }
  if (glyph_array->bloom_valid) {
    ga->bloom = glyph_array->bloom;
    ga->bloom_valid = true;
  }
  return ga;
}

const uint16_t* GlyphArray_get(GlyphArray *glyph_array, size_t *length) {
  if(length != NULL) *length = glyph_array->len;
  return glyph_array->array;
}

#if !defined(NO_FREETYPE)
static const char* utf8_to_codepoint(const char *p, unsigned *dst) {
  const unsigned char *up = (unsigned char*)p;
  unsigned res, n;
  switch (*p & 0xf0) {
    case 0xf0 :  res = *up & 0x07;  n = 3;  break;
    case 0xe0 :  res = *up & 0x0f;  n = 2;  break;
    case 0xd0 :
    case 0xc0 :  res = *up & 0x1f;  n = 1;  break;
    default   :  res = *up;         n = 0;  break;
  }
  while (n--) {
    res = (res << 6) | (*(++up) & 0x3f);
  }
  *dst = res;
  return (const char*)up + 1;
}

GlyphArray *GlyphArray_new_from_utf8(FT_Face face, const char *string, size_t len) {
  // For now use the utf8 length, which will be at least as long as the resulting glyphId array.
  // TODO: this can be as much as 4X the size we actually need, so maybe precalculate the actual size.
  GlyphArray *ga = GlyphArray_new(len);
  const char* end = string + len;
  while (string < end) {
    uint32_t codepoint;
    string = utf8_to_codepoint(string, &codepoint);
    ga->array[ga->len++] = FT_Get_Char_Index(face, codepoint);
  }
  ga->bloom_valid = false;
  return ga;
}
#endif

GlyphArray *GlyphArray_new_from_data(const uint16_t *data, size_t len) {
  GlyphArray *ga = GlyphArray_new(len);
  if (ga == NULL) return NULL;
  GlyphArray_set(ga, 0, data, len);
  return ga;
}

bool GlyphArray_compare(GlyphArray *ga1, GlyphArray *ga2) {
  if (ga1->len != ga2->len) return false;
  if (memcmp(ga1->array, ga2->array, ga1->len) != 0) return false;
  return true;
}

void GlyphArray_print(GlyphArray *ga) {
  for (size_t i = 0; i < ga->len; i++) {
    printf("%d ", ga->array[i]);
  }
  printf("\n");
}

Bloom GlyphArray_get_bloom(const GlyphArray *ga) {
  if (ga->bloom_valid) return ga->bloom;
  GlyphArray *_ga = (GlyphArray*)ga; // Discard const, because it's only fair
  for (size_t i = 0; i < _ga->len; i++) {
    _ga->bloom = add_glyphID_to_bloom(_ga->bloom, _ga->array[i]);
    // If the bloom already covers everything, we can stop...
    if (is_full_bloom(_ga->bloom)) break;
  }
  _ga->bloom_valid = true;
  return _ga->bloom;
}

#if !defined(NO_FREETYPE)
void GlyphArray_print2(FT_Face face, GlyphArray *ga) {
  for (size_t i = 0; i < ga->len; i++) {
    char name[50];
    FT_Get_Glyph_Name(face, ga->array[i], name, 50);
    printf("[%s] ", name);
  }
  printf("\n");
}

void test_GlyphArray(FT_Face face) {
  const char *string = "Hello moto";
  const char *string2 = "12345";
  GlyphArray *ga1 = GlyphArray_new_from_utf8(face, string, strlen(string));
  GlyphArray *ga1_orig = GlyphArray_new_from_GlyphArray(ga1);
  GlyphArray *ga2 = GlyphArray_new_from_utf8(face, string2, strlen(string2));
  GlyphArray_print(ga1);
  GlyphArray_print(ga2);
  GlyphArray_append(ga1, ga2->array, ga2->len);
  GlyphArray_print(ga1);
  GlyphArray *ga3 = GlyphArray_new_from_GlyphArray(ga1);
  if (!GlyphArray_compare(ga1, ga3)) {
    printf("ERROR 1\n");
  }
  GlyphArray_shrink(ga1, ga2->len);
  GlyphArray_print(ga1);
  if (!GlyphArray_compare(ga1, ga1_orig)) {
    printf("ERROR 2\n");
  }
  GlyphArray_append(ga1, ga2->array, ga2->len);
  GlyphArray_print(ga1);
  if (!GlyphArray_compare(ga1, ga3)) {
    printf("ERROR 3\n");
  }
  GlyphArray_free(ga1);
  GlyphArray_free(ga1_orig);
  GlyphArray_free(ga2);
  GlyphArray_free(ga3);

  printf("###\n");
  ga1 = GlyphArray_new_from_utf8(face, string, strlen(string));
  GlyphArray_print(ga1);
  GlyphArray_set(ga1, ga1->len, &ga1->array[6], 4);
  GlyphArray_print(ga1);
  GlyphArray_free(ga1);
}
#endif
