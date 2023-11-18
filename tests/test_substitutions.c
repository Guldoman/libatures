#include <libatures.h>
#include <stdio.h>

#include "tap.h"
#include "test_common.h"

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library lib;
static FT_Face face;
static LBT_ChainCreator *cc;

static void setup(FT_Face face, LBT_ChainCreator **cc) {
  *cc = LBT_new(face);
}

static void teardown(LBT_ChainCreator **cc) {
  LBT_destroy(*cc);
  *cc = NULL;
}

static bool test_simple_substitution(void) {
  const unsigned char features[][4] = {{'c', 'a', 'l', 't'}};
  bool result = false;
  LBT_Glyph *original = NULL, *ligated = NULL;
  LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, features, 1);
  if (c == NULL) return false;

  const char* str = "==";
  
  original = utf8_to_GlyphID(face, str, strlen(str));
  if (original == NULL) {
    goto end;
  }

  size_t out_len = 0;
  ligated = LBT_apply_chain(c, original, strlen(str), &out_len);
  if (ligated == NULL) {
    goto end;
  }

  if (out_len != 2) {
    goto end;
  }

  LBT_Glyph expected[] = { 1166, 1011 };
  for (size_t i = 0; i < sizeof(expected)/sizeof(*expected); i++) {
    if (ligated[i] != expected[i]) {
      fprintf(stderr, "%ld: %d vs %d\n", i, ligated[i], expected[i]);
      goto end;
    }
  }

  result = true;
  end:
  free(ligated);
  free(original);
  LBT_destroy_chain(c);
  return result;
}

static bool test_no_substitution(void) {
  bool result = false;
  LBT_Glyph *original = NULL, *ligated = NULL;
  LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, NULL, 0);
  if (c == NULL) return false;

  const char* str = "==";
  
  original = utf8_to_GlyphID(face, str, strlen(str));
  if (original == NULL) {
    goto end;
  }

  size_t out_len = 0;
  ligated = LBT_apply_chain(c, original, strlen(str), &out_len);
  if (ligated == NULL) {
    goto end;
  }

  if (out_len != 2) {
    goto end;
  }

  LBT_Glyph expected[] = { 710, 710 };
  for (size_t i = 0; i < sizeof(expected)/sizeof(*expected); i++) {
    if (ligated[i] != expected[i]) {
      fprintf(stderr, "%ld: %d vs %d\n", i, ligated[i], expected[i]);
      goto end;
    }
  }

  result = true;
  end:
  free(ligated);
  free(original);
  LBT_destroy_chain(c);
  return result;
}

static tap_test tests[] = {
  { "Simple substitution", test_simple_substitution, TAP_RUN },
  { "No substitution", test_no_substitution, TAP_RUN },
};

int main() {
  init_freetype(&lib);
  load_font(lib, "tests/JetBrainsMono-Regular.ttf", &face);

  setup(face, &cc);
  tap_run_tests(tests);
  teardown(&cc);
  return 0;
}
