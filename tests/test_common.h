#pragma once
#include <libatures.h>
#include <stdio.h>

#include "tap.h"
#include "test_common.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define check_err(stmt, errfunc, ...)         \
  do {                                        \
    int __err = (stmt);                       \
    if (__err != 0) {                         \
      fprintf(stderr, "error: " __VA_ARGS__); \
      const char* err_str = errfunc(__err);   \
      if (err_str != NULL) {                  \
        fprintf(stderr, ": %s", err_str);     \
      } else {                                \
        fprintf(stderr, ": code %d", __err);  \
      }                                       \
      fprintf(stderr, "\n");                  \
      exit(EXIT_FAILURE);                     \
    }                                         \
  } while (0)

static inline void init_freetype(FT_Library *lib) {
  check_err(FT_Init_FreeType(lib), FT_Error_String, "Unable to initialize freetype");
  fprintf(stderr, "Initialized freetype.\n");
}

static inline void destroy_freetype(FT_Library lib) {
  FT_Done_FreeType(lib);
}

static inline void load_font(FT_Library lib, const char* filename, FT_Face *face) {
  check_err(FT_New_Face(lib, filename, 0, face), FT_Error_String, "Unable to load font");
  fprintf(stderr, "Loaded font.\n");
}

static inline void destroy_font(FT_Face face) {
  FT_Done_Face(face);
}

static inline  const char* utf8_to_codepoint(const char *p, unsigned *dst) {
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

static inline LBT_Glyph* utf8_to_GlyphID(FT_Face face, const char *string, size_t len) {
  const char* end = string + len;
  LBT_Glyph* gIDs = malloc(sizeof(LBT_Glyph) * len);
  size_t i = 0;
  while (string < end) {
    uint32_t codepoint;
    string = utf8_to_codepoint(string, &codepoint);
    gIDs[i++] = FT_Get_Char_Index(face, codepoint);
  }
  return gIDs;
}
