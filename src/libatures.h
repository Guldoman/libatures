/** \file libatures.h
*/
#pragma once
#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_LIBATURES
    #define LIBATURES_PUBLIC __declspec(dllexport)
  #else
    #define LIBATURES_PUBLIC __declspec(dllimport)
  #endif
#else
  #ifdef BUILDING_LIBATURES
    #define LIBATURES_PUBLIC __attribute__ ((visibility ("default")))
  #else
    #define LIBATURES_PUBLIC
  #endif
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct LBT_ChainCreator LBT_ChainCreator;
typedef struct LBT_Chain LBT_Chain;
typedef uint16_t LBT_Glyph;

#if !defined(NO_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H

/**
 * \brief Create an LBT_ChainCreator from a FreeType font face.
 *
 * \param[in] face
 */
LBT_ChainCreator LIBATURES_PUBLIC *LBT_new(FT_Face face);
#endif

/**
 * \brief Create an LBT_ChainCreator from a given GSUB table.
 *
 * The tables passed to this function will be fully managed by `libatures`, and
 * will be freed by ::LBT_destroy.
 *
 * \see ::LBT_new
 *
 * \param[in] GSUB_table
 */
LBT_ChainCreator LIBATURES_PUBLIC *LBT_new_from_tables(uint8_t *GSUB_table);


/**
 * \brief Destroy an LBT_ChainCreator
 *
 * Call this only after every LBT_Chain generated from the specified
 * LBT_ChainCreator is destroyed.
 *
 * \param[in,out] cc
 */
void LIBATURES_PUBLIC LBT_destroy(LBT_ChainCreator* cc);


/**
 * \brief Generate a chain of glyph substitutions.
 *
 * No feature is enabled by default, so you must specify all of them.
 *
 * The feature order doesn't have any effect on the output as Lookups are
 * executed in Lookup order.
 *
 * To use the required feature, if any, use the feature tag \c{' ', 'R', 'Q', 'D'}\c.
 *
 * Needs to be destroyed by ::LBT_destroy_chain.
 *
 * \param[in] cc
 * \param[in] script Set to `NULL` to use the default script
 * \param[in] lang Set to `NULL` to use the default language
 * \param[in] features
 * \param[in] n_features
 */
LBT_Chain LIBATURES_PUBLIC *LBT_generate_chain(const LBT_ChainCreator *cc,
                                               const unsigned char (*script)[4],
                                               const unsigned char (*lang)[4],
                                               const unsigned char (*features)[4],
                                               size_t n_features);

/**
 * \brief Apply chain to an `LBT_Glyph` array.
 *
 * This function is not thread-safe. Create multiple chains to execute them in
 * parallel.
 *
 * \param[in] chain
 * \param[in] glyph_array Array of glyphs to "ligate".
 * \param[in] n_input_glyphs Number of glyphs in `glyph_array`.
 * \param[out] n_output_glyphs Number of glyphs returned.
 * \return Array of "ligated" glyphs.
 */
LBT_Glyph LIBATURES_PUBLIC *LBT_apply_chain(const LBT_Chain *chain,
                                            const LBT_Glyph* glyph_array,
                                            size_t n_input_glyphs,
                                            size_t *n_output_glyphs);

/**
 * \brief Destroy a Chain.
 *
 * This will not free the relative FreeType tables.
 * The generated `GlyphArray`s are still usable.
 *
 * \param[in,out] chain
 */
void LIBATURES_PUBLIC LBT_destroy_chain(LBT_Chain *chain);
