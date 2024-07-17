#include <stdint.h>
#include <assert.h>

#include "libatures.h"
#include "gsub.h"

typedef struct LBT_ChainCreator {
  uint8_t *GSUB_table;
} LBT_ChainCreator;

LBT_ChainCreator *LBT_new_from_tables(uint8_t *GSUB_table) {
  LBT_ChainCreator *cc = malloc(sizeof(LBT_ChainCreator));
  if (cc == NULL) {
    return NULL;
  }
  cc->GSUB_table = GSUB_table;
  return cc;
}

#if !defined(NO_FREETYPE)
#include <ft2build.h>
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H
#include FT_FREETYPE_H

static FT_Error get_table(FT_Face face, FT_ULong tag, uint8_t **table) {
  FT_Error error;
  FT_ULong table_len = 0;
  *table = NULL;
  // Get size only
  error = FT_Load_Sfnt_Table(face, tag, 0, NULL, &table_len);
  if (error == FT_Err_Table_Missing) {
    return FT_Err_Ok;
  }
  if (error) {
    return error;
  }

  *table = (uint8_t *)malloc(table_len);
  if (table == NULL) {
    return FT_Err_Out_Of_Memory;
  }

  error = FT_Load_Sfnt_Table(face, TTAG_GSUB, 0, *table, &table_len);

  return error;
}

LBT_ChainCreator *LBT_new(FT_Face face) {
  uint8_t *GSUB_table = NULL;
  if (get_table(face, TTAG_GSUB, &GSUB_table) != 0) {
    return NULL;
  }

  LBT_ChainCreator *cc = LBT_new_from_tables(GSUB_table);
  if (cc == NULL) {
    free(GSUB_table);
    return NULL;
  }

  return cc;
}
#endif

void LBT_destroy(LBT_ChainCreator* cc) {
  if (cc->GSUB_table != NULL) {
    free(cc->GSUB_table);
  }
  free(cc);
}

LBT_Chain *LBT_generate_chain(const LBT_ChainCreator *cc, LBT_tag *script, LBT_tag *lang, LBT_tag *features, size_t n_features) {
  return generate_chain(cc->GSUB_table, script, lang, features, n_features);
}

void LBT_destroy_chain(LBT_Chain *chain) {
  destroy_chain(chain);
}

LBT_Glyph* LBT_apply_chain(const LBT_Chain *chain, const LBT_Glyph* glyph_array, size_t n_input_glyphs, size_t *n_output_glyphs) {
  GlyphArray *ga = GlyphArray_new_from_data(glyph_array, n_input_glyphs);

  apply_chain(chain, ga);

  LBT_Glyph* out = ga->array;
  ga->array = NULL;

  if (n_output_glyphs != NULL) *n_output_glyphs = ga->len;

  GlyphArray_free(ga);
  return out;
}
