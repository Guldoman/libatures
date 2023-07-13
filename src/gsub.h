#ifndef GSUB_H
#define GSUB_H
#include <stdint.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_FREETYPE_H


#ifdef __clang__
  #define packed_BE __attribute__((__packed__))
  #warning "Wrong endianess"
#else
  #define packed_BE __attribute__((__packed__, scalar_storage_order("big-endian")))
#endif

typedef struct packed_BE {
  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t scriptListOffset;
  uint16_t featureListOffset;
  uint16_t lookupListOffset;
} GsubHeader;

/** Script **/
typedef struct packed_BE {
  uint8_t scriptTag[4];
  uint16_t scriptOffset;
} ScriptRecord;

typedef struct packed_BE  {
  uint16_t scriptCount;
  ScriptRecord scriptRecords[];
} ScriptList;

typedef struct packed_BE {
  uint8_t langSysTag[4];
  uint16_t langSysOffset;
} LangSysRecord;

typedef struct packed_BE {
  uint16_t defaultLangSysOffset; // 0 means no default
  uint16_t langSysCount;
  LangSysRecord langSysRecords[];
} ScriptTable;

typedef struct packed_BE {
  uint16_t lookupOrderOffset; // NULL - reeserved
  uint16_t requiredFeatureIndex; // 0xFFFF for no required features
  uint16_t featureIndexCount;
  uint16_t featureIndices[];
} LangSysTable;

/** Feature **/
typedef struct packed_BE {
  uint8_t featureTag[4];
  uint16_t featureOffset;
} FeatureRecord;

typedef struct packed_BE {
  uint16_t featureCount;
  FeatureRecord featureRecords[];
} FeatureList;

typedef struct packed_BE {
  uint16_t featureParamsOffset; // used for 'cv01' – 'cv99', 'size', 'ss01' – 'ss20'
  uint16_t lookupIndexCount;
  uint16_t lookupListIndices[];
} FeatureTable;

/** Lookup **/
typedef struct packed_BE {
  uint16_t lookupCount;
  uint16_t lookupOffsets[];
} LookupList;
typedef struct packed_BE {
  uint16_t lookupType;
  uint16_t lookupFlag;
  uint16_t subTableCount;
  uint16_t subtableOffsets[];
  // uint16_t markFilteringSet;
} LookupTable;

/** Generic Substitution Table **/
typedef struct packed_BE {
  uint16_t substFormat;
} GenericSubstTable;

/** Single Substitution Format **/
typedef struct packed_BE {
  uint16_t substFormat;
  uint16_t coverageOffset;
} SingleSubstFormatGeneric;

typedef struct packed_BE {
  uint16_t substFormat;
  uint16_t coverageOffset;
  int16_t deltaGlyphID;
} SingleSubstFormat1;

typedef struct packed_BE {
  uint16_t substFormat;
  uint16_t coverageOffset;
  int16_t glyphCount;
  int16_t substituteGlyphIDs[];
} SingleSubstFormat2;

/** Multiple Substitution Format **/
typedef struct packed_BE {
  uint16_t substFormat;
  uint16_t coverageOffset;
  uint16_t sequenceCount;
  uint16_t sequenceOffsets[];
} MultipleSubstFormat1;

typedef struct packed_BE {
  uint16_t glyphCount;
  uint16_t substituteGlyphIDs[];
} SequenceTable;

/** Ligature Substitution Format **/
typedef struct packed_BE {
  uint16_t substFormat; // Set to 1 maybe?
  uint16_t coverageOffset;
  uint16_t ligatureSetCount;
  uint16_t ligatureSetOffsets[];
} LigatureSubstitutionTable;
typedef struct packed_BE {
  uint16_t coverageFormat;
  uint16_t count;
  uint16_t array;
} CoverageTable;
typedef struct packed_BE {
  uint16_t coverageFormat;
  uint16_t glyphCount;
  uint16_t glyphArray[];
} CoverageArrayTable;
typedef struct packed_BE {
  uint16_t startGlyphID;
  uint16_t endGlyphID;
  uint16_t startCoverageIndex;
} CoverageRangeRecordTable;
typedef struct packed_BE {
  uint16_t coverageFormat;
  uint16_t rangeCount;
  CoverageRangeRecordTable rangeRecords[];
} CoverageRangesTable;
typedef struct packed_BE {
  uint16_t ligatureCount;
  uint16_t ligatureOffsets[];
} LigatureSetTable;
typedef struct packed_BE {
  uint16_t ligatureGlyph;
  uint16_t componentCount;
  uint16_t componentGlyphIDs[];
} LigatureTable;

typedef struct packed_BE {
  uint16_t substFormat;
  uint16_t extensionLookupType;
  uint32_t extensionOffset;
} ExtensionSubstitutionTable;

typedef struct {
  FT_Face face;
  LookupTable **lookupsArray;
  size_t lookupCount;
  FT_Bytes GSUB_table;
} Chain;

typedef struct {
  size_t len;
  size_t allocated;
  uint16_t *array;
} GlyphArray;

void parse_GSUB(FT_Face face, const char* path);

bool get_required_feature(const FT_Face face, const char (*script)[4], const char (*lang)[4], char (*required_feature)[4]);
Chain *generate_chain(FT_Face face, const char (*script)[4], const char (*lang)[4], const char (*features)[4], size_t n_features);
void destroy_chain(Chain *chain);
GlyphArray *apply_chain(const Chain *chain, const GlyphArray* glyph_array);

GlyphArray *GlyphArray_new(size_t size);
GlyphArray *GlyphArray_new_from_utf8(FT_Face face, const char *string, size_t len);
bool GlyphArray_append(GlyphArray *glyph_array, const uint16_t *data, size_t data_size);
void GlyphArray_free(GlyphArray *ga);
void GlyphArray_print(GlyphArray *ga);
void GlyphArray_print2(FT_Face face, GlyphArray *ga);

void test_GlyphArray(FT_Face face);

#endif // GSUB_H
