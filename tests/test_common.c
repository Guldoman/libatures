#include "test_common.h"

void init_freetype(FT_Library *lib) {
  check_err(FT_Init_FreeType(lib), FT_Error_String, "Unable to initialize freetype");
  fprintf(stderr, "Initialized freetype.\n");
}

void destroy_freetype(FT_Library lib) {
  FT_Done_FreeType(lib);
}

void load_font(FT_Library lib, const char* filename, FT_Face *face) {
  check_err(FT_New_Face(lib, filename, 0, face), FT_Error_String, "Unable to load font");
  fprintf(stderr, "Loaded font.\n");
}

void destroy_font(FT_Face face) {
  FT_Done_Face(face);
}

const char* utf8_to_codepoint(const char *p, unsigned *dst) {
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

LBT_Glyph* utf8_to_GlyphID(FT_Face face, const char *string, size_t len) {
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

void print_got_vs_expected(LBT_Glyph *ligated, size_t n_ligated, LBT_Glyph *expected, size_t n_expected) {
  fprintf(stderr, "Expected: ");
  for (size_t i = 0; i < n_expected; i++) {
    fprintf(stderr, "\t%d", expected[i]);
  }
  fprintf(stderr, "\nGot:      ");
  for (size_t i = 0; i < n_ligated; i++) {
    fprintf(stderr, "\t%d", ligated[i]);
  }
  fprintf(stderr, "\n\n");
}

bool test_sub(LBT_ChainCreator *cc, FT_Face face, LBT_tag *script, LBT_tag *lang, LBT_tag *features, size_t n_features, const char *text, LBT_Glyph *expected_glyphs, size_t n_expected_glyphs) {
  bool result = false;
  LBT_Glyph *original = NULL, *ligated = NULL;
  LBT_Chain *c = LBT_generate_chain(cc, script, lang, features, n_features);
  if (c == NULL) {
    fprintf(stderr, "Unable to generate chain");
    return false;
  }
  
  original = utf8_to_GlyphID(face, text, strlen(text));
  if (original == NULL) {
    fprintf(stderr, "Unable to convert text to glyphs\n");
    goto end;
  }

  size_t out_len = 0;
  ligated = LBT_apply_chain(c, original, strlen(text), &out_len);
  if (ligated == NULL) {
    fprintf(stderr, "No glyphs were returned\n");
    goto end;
  }

  if (out_len != n_expected_glyphs) {
    fprintf(stderr, "Expected %ld glyphs, got %ld\n", n_expected_glyphs, out_len);
    print_got_vs_expected(ligated, out_len, expected_glyphs, n_expected_glyphs);
    goto end;
  }

  for (size_t i = 0; i < n_expected_glyphs; i++) {
    if (ligated[i] != expected_glyphs[i]) {
      fprintf(stderr, "Got wrong glyph at position %ld: %d vs %d\n", i, ligated[i], expected_glyphs[i]);
      print_got_vs_expected(ligated, out_len, expected_glyphs, n_expected_glyphs);
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
