#pragma once
#include <libatures.h>
#include <stdint.h>
#include <stdbool.h>

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

#define SCRIPT(s)\
LBT_tag __script = s;\
LBT_tag *_script = &__script;
#define NO_SCRIPT()\
LBT_tag *_script = NULL;

#define LANG(l)\
LBT_tag __lang = l;\
LBT_tag *_lang = &__lang;
#define NO_LANG()\
LBT_tag *_lang = NULL;

#define FEATS(...)\
LBT_tag _features[] = { __VA_ARGS__ };

#define EXPECTED(...)\
LBT_Glyph _expected[] = { __VA_ARGS__ };

#define make_test(name, script, lang, features, text, expected)\
static bool test_##name(void) {\
  script\
  lang\
  features\
  expected\
  return test_sub(cc, face,\
                  _script, _lang,\
                  _features, sizeof(_features)/sizeof(_features[0]),\
                  (text),\
                  _expected, sizeof(_expected)/sizeof(_expected[0]));\
}

void init_freetype(FT_Library *lib);
void destroy_freetype(FT_Library lib);

void load_font(FT_Library lib, const char* filename, FT_Face *face);
void destroy_font(FT_Face face);

const char* utf8_to_codepoint(const char *p, unsigned *dst);
LBT_Glyph* utf8_to_GlyphID(FT_Face face, const char *string, size_t len);

void print_got_vs_expected(LBT_Glyph *ligated, size_t n_ligated, LBT_Glyph *expected, size_t n_expected);

bool test_sub(LBT_ChainCreator *cc, FT_Face face, LBT_tag *script, LBT_tag *lang, LBT_tag *features, size_t n_features, const char *text, LBT_Glyph *expected_glyphs, size_t n_expected_glyphs);
