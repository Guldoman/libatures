#pragma once
#include <stdint.h>
#include <stdbool.h>

#if !defined(NO_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include "bloom.h"

// typedef struct GlyphArray GlyphArray;
typedef struct GlyphArray {
  size_t len;
  size_t allocated;
  uint16_t *array;
  Bloom bloom;
  bool bloom_valid;
} GlyphArray;

GlyphArray *GlyphArray_new(size_t size);
#if !defined(NO_FREETYPE)
GlyphArray *GlyphArray_new_from_utf8(FT_Face face, const char *string, size_t len);
#endif
GlyphArray *GlyphArray_new_from_data(uint16_t *data, size_t len);
GlyphArray *GlyphArray_new_from_GlyphArray(const GlyphArray *glyph_array);
const uint16_t* GlyphArray_get(GlyphArray *glyph_array, size_t *length);
Bloom GlyphArray_get_bloom(const GlyphArray *ga);
bool GlyphArray_set1(GlyphArray *glyph_array, size_t index, uint16_t data);
bool GlyphArray_set(GlyphArray *glyph_array, size_t from, const uint16_t *data, size_t data_size);
bool GlyphArray_append(GlyphArray *glyph_array, const uint16_t *data, size_t data_size);
bool GlyphArray_put(GlyphArray *dst, size_t dst_index, GlyphArray *src, size_t src_index, size_t len);
bool GlyphArray_shrink(GlyphArray *glyph_array, size_t reduction);
bool GlyphArray_compare(GlyphArray *ga1, GlyphArray *ga2);
void GlyphArray_free(GlyphArray *ga);
void GlyphArray_print(GlyphArray *ga);
#if !defined(NO_FREETYPE)
void GlyphArray_print2(FT_Face face, GlyphArray *ga);

void test_GlyphArray(FT_Face face);
#endif
