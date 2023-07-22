#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <ft2build.h>
#include FT_OPENTYPE_VALIDATE_H
#include FT_FREETYPE_H

#include "gsub.h"

GlyphArray *GlyphArray_new(size_t size) {
  GlyphArray *ga = malloc(sizeof(GlyphArray));
  if (ga == NULL) return NULL;
  ga->len = 0;
  ga->allocated = size;
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


bool GlyphArray_set(GlyphArray *glyph_array, size_t from, const uint16_t *data, size_t data_size) {
  GlyphArray *ga = glyph_array;
  if (from > ga->len) {
    // TODO: error out maybe?
    return false;
  }
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
  memmove(&ga->array[from], data, data_size * sizeof(uint16_t));
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
  ga->len -= reduction;
  return true;
}


bool GlyphArray_append(GlyphArray *glyph_array, const uint16_t *data, size_t data_size) {
  return GlyphArray_set(glyph_array, glyph_array->len, data, data_size);
}

static GlyphArray * GlyphArray_new_from_GlyphArray(const GlyphArray *glyph_array) {
  GlyphArray *ga = GlyphArray_new(glyph_array->len);
  if (ga == NULL) return NULL;
  if (!GlyphArray_append(ga, glyph_array->array, glyph_array->len)) {
    GlyphArray_free(ga);
    return NULL;
  }
  return ga;
}

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
  return ga;
}

GlyphArray *GlyphArray_new_from_data(uint16_t *data, size_t len) {
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

void GlyphArray_print2(FT_Face face, GlyphArray *ga) {
  for (size_t i = 0; i < ga->len; i++) {
    char name[50];
    FT_Get_Glyph_Name(face, ga->array[i], name, 50);
    printf("[%s] ", name);
  }
  printf("\n");
}

void test_GlyphArray(FT_Face face) {
  char *string = "Hello moto";
  char *string2 = "12345";
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


/** +++++++++++++++++++++++ **/


#define compare_tags(tag1, tag2) ((tag1)[0] == (tag2)[0] && (tag1)[1] == (tag2)[1] && (tag1)[2] == (tag2)[2] && (tag1)[3] == (tag2)[3])

const char DFLT_tag[4] = {'D', 'F', 'L', 'T'};
const char dflt_tag[4] = {'d', 'f', 'l', 't'};
const char _RQD_tag[4] = {' ', 'R', 'Q', 'D'};
const char latn_tag[4] = {'l', 'a', 't', 'n'};

enum {
  SingleLookupType = 1,
  MultipleLookupType,
  AlternateLookupType,
  LigatureLookupType,
  ContextLookupType,
  ChainingLookupType,
  ExtensionSubstitutionLookupType,
  ReverseChainingContextSingleLookupType,
  ReservedLookupType
} LookupTypes;

enum {
  SingleSubstitutionFormat_1 = 1,
  SingleSubstitutionFormat_2
} SingleSubstitutionFormats;

enum {
  SequenceContextFormat_1 = 1,
  SequenceContextFormat_2,
  SequenceContextFormat_3
} SequenceContextFormats;

enum {
  ChainedSequenceContextFormat_1 = 1,
  ChainedSequenceContextFormat_2,
  ChainedSequenceContextFormat_3
} ChainedSequenceContextFormats;

enum {
  ClassFormat_1 = 1,
  ClassFormat_2
} ClassFormats;

enum {
  ReverseChainSingleSubstFormat_1 = 1,
} ReverseChainSingleSubstFormat;

// Return the ScriptTable with the specified tag.
// If the tag is NULL, the default script is returned.
static const ScriptTable *get_script_table(const ScriptList *scriptList, const char (*script)[4]) {
  for (uint16_t i = 0; i < scriptList->scriptCount; i++) {
    const ScriptRecord *scriptRecord = &scriptList->scriptRecords[i];
    const ScriptTable *scriptTable = (ScriptTable *)((uint8_t *)scriptList + scriptRecord->scriptOffset);
    if (script == NULL) {
      // In general, the uppercase version should be the one used
      if (compare_tags(DFLT_tag, scriptRecord->scriptTag)
          || compare_tags(dflt_tag, scriptRecord->scriptTag))
        return scriptTable;
    }
    else if (compare_tags(scriptRecord->scriptTag, *script)) {
      return scriptTable;
    }
  }
  return (ScriptTable *)NULL;
}

// Return the LangSysTable with the specified tag.
// If the tag is NULL, the default language for the script is returned.
static const LangSysTable *get_lang_table(const ScriptTable *scriptTable, const char (*lang)[4]) {
  // Try to use the default language
  if (lang == NULL || compare_tags(DFLT_tag, *lang) || compare_tags(dflt_tag, *lang)) {
    if (scriptTable->defaultLangSysOffset != 0) {
      return (LangSysTable *)((uint8_t *)scriptTable + scriptTable->defaultLangSysOffset);
    }
  }

  // If the default language wasn't defined, try looking for the language with the dflt tag.
  // In theory dflt (and DFLT) should never appear as language tags, but here we are...
  if (lang == NULL) {
    const LangSysTable *result = get_lang_table(scriptTable, &dflt_tag);
    if (result == NULL)
      result = get_lang_table(scriptTable, &DFLT_tag);
    return result;
  }

  for (uint16_t i = 0; i < scriptTable->langSysCount; i++) {
    const LangSysRecord *langSysRecord = &scriptTable->langSysRecords[i];
    if (compare_tags(langSysRecord->langSysTag, *lang)) {
      return (LangSysTable *)((uint8_t *)scriptTable + langSysRecord->langSysOffset);
    }
  }

  return (LangSysTable *)NULL;
}

// Return the FeatureTable from the FeatureList at the specified index.
// If featureTag is not NULL, it's set to the tag of the feature.
static const FeatureTable *get_feature(const FeatureList *featureList, uint16_t index, char (*featureTag)[4]) {
  if (index > featureList->featureCount) return (FeatureTable *)NULL;

  const FeatureRecord *featureRecord = &(featureList->featureRecords[index]);
  const FeatureTable *featureTable = (FeatureTable *)((uint8_t *)featureList + featureRecord->featureOffset);
  if (featureTag != NULL) {
    (*featureTag)[0] = featureRecord->featureTag[0];
    (*featureTag)[1] = featureRecord->featureTag[1];
    (*featureTag)[2] = featureRecord->featureTag[2];
    (*featureTag)[3] = featureRecord->featureTag[3];
  }
  return featureTable;
}

static LookupTable *get_lookup(const LookupList *lookupList, uint16_t index) {
  if (index > lookupList->lookupCount) return (LookupTable *) NULL;
  return (LookupTable *)((uint8_t *)lookupList + lookupList->lookupOffsets[index]);
}


// Returns the number of lookups inside the FeatureTable.
// lookups must either be an array big enough to contain
// all the pointers to lookups, or be NULL.
static size_t get_lookups_from_feature(const FeatureTable *featureTable, const LookupList *lookupList, bool *lookups_map) {
  size_t c = 0;
  for (uint16_t k = 0; k < featureTable->lookupIndexCount; k++) {
    LookupTable *lookup = get_lookup(lookupList, featureTable->lookupListIndices[k]);
    if (lookup == NULL) {
      fprintf(stderr, "Warning: unable to obtain lookup of feature\n");
      // Try with next feature
      continue;
    }
    if (!lookups_map[featureTable->lookupListIndices[k]]) {
      lookups_map[featureTable->lookupListIndices[k]] = true;
      c++;
    }
  }
  return c;
}

// Returns the number of lookups of the given LangSysTable,
// filtered and sorted as specified in features_enabled.
// lookups must either be an array big enough to contain
// all the pointers to lookups, or be NULL.
static size_t get_lookups(const LangSysTable* langSysTable, const FeatureList *featureList, const LookupList *lookupList, const char (*features_enabled)[4], size_t nFeatures, LookupTable ***lookups) {
  bool *lookups_map = calloc(lookupList->lookupCount, sizeof(bool));
  if (lookups_map == NULL) return 0;

  size_t c = 0;
  if (features_enabled == NULL) return 0;

  for (size_t i = 0; i < nFeatures; i++) {
    if (langSysTable->requiredFeatureIndex != 0xFFFF && compare_tags(_RQD_tag, features_enabled[i])) {
      const FeatureTable *featureTable = get_feature(featureList, langSysTable->requiredFeatureIndex, NULL);
      c += get_lookups_from_feature(featureTable, lookupList, lookups_map);
      continue;
    }
    for (uint16_t j = 0; j < langSysTable->featureIndexCount; j++) {
      char featureTag[4];
      uint16_t index = langSysTable->featureIndices[j];
      const FeatureTable *featureTable = get_feature(featureList, index, &featureTag);
      if (featureTable == NULL) {
        fprintf(stderr, "Warning: unable to obtain feature#%d\n", index);
        // Try with next feature
        continue;
      }
      if (compare_tags(featureTag, features_enabled[i])) {
        // There should be only one feature with the same tag
        // TODO: check if this is actually the case
        c += get_lookups_from_feature(featureTable, lookupList, lookups_map);
        break;
      }
    }
  }
  if (lookups != NULL) {
    LookupTable **_lookups = malloc(sizeof(LookupTable *) * c);
    if (_lookups == NULL) {
      *lookups = NULL;
      goto end;
    }
    for (uint16_t i = 0, j = 0; i < lookupList->lookupCount; i++) {
      if (lookups_map[i]) {
        _lookups[j] = get_lookup(lookupList, i);
        ///printf("%d -> %d\n", j, i);
        j++;
      }
    }
    *lookups = _lookups;
  }
end:
  free(lookups_map);
  return c;
}

// Generates a chain of Lookups to apply in order, given the script and language selected,
// as well as the features enabled.
// script and lang can be NULL to select the default ones.
// features specifies the list of features to apply, and in which order.
// Some fonts specify required features; use the tag `{' ', 'R', 'Q', 'D'}` in the features
// to specify where it belongs in the chain.
// Use `get_required_feature` if the tag is needed to decide where to place it.
Chain *generate_chain(FT_Face face, const char (*script)[4], const char (*lang)[4], const char (*features)[4], size_t n_features) {
  FT_Bytes GSUB_table, ignore;
  LookupTable **lookupsArray = NULL;
  // Need to pass the ignore table to make FreeType happy.
  if (FT_OpenType_Validate(face, FT_VALIDATE_GSUB, &ignore, &ignore, &ignore, &GSUB_table, &ignore) != 0) {
    return NULL;
  }
  FT_OpenType_Free(face, ignore);

  if (GSUB_table == NULL) {
    // There is no GSUB table.
    // TODO: should this just be an empty chain?
    goto fail;
  }

  const GsubHeader *gsubHeader = (GsubHeader *)GSUB_table;

  const ScriptList *scriptList = (ScriptList *)((uint8_t *)gsubHeader + gsubHeader->scriptListOffset);
  const ScriptTable *scriptTable = get_script_table(scriptList, script);
  // Try again with latn, as some fonts don't define the default script.
  if (scriptTable == NULL && script == NULL) {
    scriptTable = get_script_table(scriptList, &latn_tag);
  }
  if (scriptTable == NULL) {
    goto fail;
  }

  const LangSysTable *langSysTable = get_lang_table(scriptTable, lang);
  if (langSysTable == NULL) {
    goto fail;
  }

  const FeatureList *featureList = (FeatureList *)((uint8_t *)gsubHeader + gsubHeader->featureListOffset);
  const LookupList *lookupList = (LookupList *)((uint8_t *)gsubHeader + gsubHeader->lookupListOffset);

  size_t lookupCount = get_lookups(langSysTable, featureList, lookupList, features, n_features, &lookupsArray);

  Chain *chain = malloc(sizeof(Chain));
  if (chain == NULL)
    goto fail;
  chain->face = face;
  chain->lookupsArray = lookupsArray;
  chain->lookupCount = lookupCount;
  chain->GSUB_table = GSUB_table;
  return chain;

fail:
  FT_OpenType_Free(face, GSUB_table);
  if (lookupsArray != NULL) free(lookupsArray);
  return NULL;
}

void destroy_chain(Chain *chain) {
  if (chain == NULL) return;
  FT_OpenType_Free(chain->face, chain->GSUB_table);
  free(chain->lookupsArray);
  free(chain);
}

// Returns whether the specified script and language combo has a required feature.
// `script` and `lang` can be NULL to select the default ones.
// Writes in `required_feature` the tag.
bool get_required_feature(const FT_Face face, const char (*script)[4], const char (*lang)[4], char (*required_feature)[4]) {
  FT_Bytes GSUB_table, ignore;
  bool result = false;
  // Need to pass the ignore table to make FreeType happy
  if (FT_OpenType_Validate(face, FT_VALIDATE_GSUB, &ignore, &ignore, &ignore, &GSUB_table, &ignore) != 0) {
    return false;
  }
  FT_OpenType_Free(face, ignore);

  if (GSUB_table == NULL) {
    goto end;
  }

  const GsubHeader *gsubHeader = (GsubHeader *)GSUB_table;
  const ScriptList *scriptList = (ScriptList *)((uint8_t *)gsubHeader + gsubHeader->scriptListOffset);
  const ScriptTable *scriptTable = get_script_table(scriptList, script);
  if (scriptTable == NULL) {
    goto end;
  }
  const LangSysTable *langSysTable = get_lang_table(scriptTable, lang);
  if (langSysTable == NULL) {
    goto end;
  }

  // 0xFFFF means no required feature.
  if (langSysTable->requiredFeatureIndex != 0xFFFF) {
    const FeatureList *featureList = (FeatureList *)((uint8_t *)gsubHeader + gsubHeader->featureListOffset);
    const FeatureTable *featureTable = get_feature(featureList, langSysTable->requiredFeatureIndex, required_feature);
    if (featureTable != NULL) {
      result = true;
    }
  }

end:
  FT_OpenType_Free(face, GSUB_table);
  return result;
}


static bool find_in_coverage(const CoverageTable *coverageTable, uint16_t id, uint32_t *index) {
  switch (coverageTable->coverageFormat) {
    case 1: { // Individual glyph indices
      const CoverageArrayTable *arrayTable = (CoverageArrayTable *)coverageTable;
      for (uint16_t i = 0; i < arrayTable->glyphCount; i++) {
        if (id == arrayTable->glyphArray[i]) {
          if (index != NULL) {
            *index = i;
          }
          return true;
        }
        // The glyphArray contains glyphIDs in numerical order,
        // and we passed out ID.
        if (id < arrayTable->glyphArray[i]) {
          break;
        }
      }
      break;
    }
    case 2: { // Range of glyphs
      const CoverageRangesTable *rangesTable = (CoverageRangesTable *)coverageTable;
      if (rangesTable->rangeCount == 0) return false;
      const CoverageRangeRecordTable *first_range = &(rangesTable->rangeRecords[0]);
      if (first_range->startGlyphID > id) return false;
      const CoverageRangeRecordTable *last_range = &(rangesTable->rangeRecords[rangesTable->rangeCount - 1]);
      if (last_range->endGlyphID < id) return false;
      uint16_t k = 0;
      for (uint16_t i = 0; i < rangesTable->rangeCount; i++) {
        const CoverageRangeRecordTable *range = &rangesTable->rangeRecords[i];
        if (id < range->startGlyphID) break;
        if (id > range->endGlyphID) {
          k += range->endGlyphID - range->startGlyphID + 1;
          continue;
        }
        for (uint16_t j = range->startGlyphID; j <= id; j++) {
          if (id == j) {
            if (index != NULL) {
              *index = k;
            }
            return true;
          }
          k++;
        }
      }
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN coverage format\n");
      return false;
  }
  return false;
}


static bool find_in_class_array(const ClassDefGeneric *classDefTable, uint16_t id, uint16_t *class) {
  switch (classDefTable->classFormat) {
    case ClassFormat_1: { // Individual glyph indices
      ClassDefFormat1 *arrayTable = (ClassDefFormat1 *)classDefTable;
      if (id < arrayTable->startGlyphID || id >= arrayTable->startGlyphID + arrayTable->glyphCount) {
        break;
      }
      if (class != NULL) {
        *class = arrayTable->classValueArray[id - arrayTable->startGlyphID];
      }
      return true;
    }
    case ClassFormat_2: { // Range of glyphs
      ClassDefFormat2 *rangesTable = (ClassDefFormat2 *)classDefTable;
      if (rangesTable->classRangeCount == 0) break;
      const ClassRangeRecord *first_range = &(rangesTable->classRangeRecords[0]);
      if (first_range->startGlyphID > id) break;
      const ClassRangeRecord *last_range = &(rangesTable->classRangeRecords[rangesTable->classRangeCount - 1]);
      if (last_range->endGlyphID < id) break;

      for (uint16_t i = 0; i < rangesTable->classRangeCount; i++) {
        const ClassRangeRecord *range = &rangesTable->classRangeRecords[i];
        if (id < range->startGlyphID) break;
        if (id > range->endGlyphID) {
          continue;
        }
        for (uint16_t j = range->startGlyphID; j <= id; j++) {
          if (id == j) {
            if (class != NULL) {
              *class = range->_class;
            }
            return true;
          }
        }
      }
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN class format\n");
      break;
  }
  if (class != NULL) {
    *class = 0;
  }
  return false;
}

static bool apply_SingleSubstitution(SingleSubstFormatGeneric *singleSubstFormatGeneric, GlyphArray* glyph_array, size_t index) {
  CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)singleSubstFormatGeneric + singleSubstFormatGeneric->coverageOffset);
  switch (singleSubstFormatGeneric->substFormat) {
    case SingleSubstitutionFormat_1: {
      SingleSubstFormat1 *singleSubst = (SingleSubstFormat1 *)singleSubstFormatGeneric;
      if (find_in_coverage(coverageTable, glyph_array->array[index], NULL)) {
        glyph_array->array[index] += singleSubst->deltaGlyphID;
        return true;
      }
      break;
    }
    case SingleSubstitutionFormat_2: {
      SingleSubstFormat2 *singleSubst = (SingleSubstFormat2 *)singleSubstFormatGeneric;
      uint32_t coverage_index;
      if (find_in_coverage(coverageTable, glyph_array->array[index], &coverage_index)) {
        glyph_array->array[index] = singleSubst->substituteGlyphIDs[coverage_index];
        return true;
      }
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN SubstFormat %d\n", singleSubstFormatGeneric->substFormat);
      break;
  }
  return false;
}

static bool apply_MultipleSubstitution(const MultipleSubstFormat1 *multipleSubstFormat, GlyphArray* glyph_array, size_t *index) {
  CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)multipleSubstFormat + multipleSubstFormat->coverageOffset);
  uint32_t coverage_index;
  bool applicable = find_in_coverage(coverageTable, glyph_array->array[*index], &coverage_index);
  if (!applicable || coverage_index >= multipleSubstFormat->sequenceCount) return false;

  SequenceTable *sequenceTable = (SequenceTable *)((uint8_t *)multipleSubstFormat + multipleSubstFormat->sequenceOffsets[coverage_index]);
  GlyphArray_put(glyph_array, *index + sequenceTable->glyphCount - 1, glyph_array, *index, glyph_array->len - *index - 1);
  for (uint16_t j = 0; j < sequenceTable->glyphCount; j++) {
    glyph_array->array[*index + j] = sequenceTable->substituteGlyphIDs[j];
  }
  *index += sequenceTable->glyphCount - 1; // ++ will be done by apply_Lookup
  return true;
}

static LigatureTable *find_Ligature(LigatureSetTable *ligatureSet, GlyphArray* glyph_array, size_t index) {
  for (uint16_t i = 0; i < ligatureSet->ligatureCount; i++) {
    LigatureTable *ligature = (LigatureTable *)((uint8_t *)ligatureSet + ligatureSet->ligatureOffsets[i]);
    bool fail = false;
    if (index + ligature->componentCount - 1 > glyph_array->len) continue;
    for (uint16_t j = 0; j < ligature->componentCount - 1; j++) {
      if (ligature->componentGlyphIDs[j] != glyph_array->array[index+j]) {
        fail = true;
        break;
      }
    }
    if (!fail) {
      return ligature;
    }
  }
  return (LigatureTable *)NULL;
}

static bool apply_LigatureSubstitution(LigatureSubstitutionTable *ligatureSubstitutionTable, GlyphArray* glyph_array, size_t index) {
  CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)ligatureSubstitutionTable + ligatureSubstitutionTable->coverageOffset);
  uint32_t coverage_index;
  bool applicable = find_in_coverage(coverageTable, glyph_array->array[index], &coverage_index);
  if (!applicable || coverage_index >= ligatureSubstitutionTable->ligatureSetCount) return false;
  LigatureSetTable *ligatureSet = (LigatureSetTable *)((uint8_t *)ligatureSubstitutionTable + ligatureSubstitutionTable->ligatureSetOffsets[coverage_index]);
  // printf("Checking %d\n", glyph_array->array[index]);
  LigatureTable *ligature = find_Ligature(ligatureSet, glyph_array, index + 1);
  if (ligature != NULL) {
    // printf("FOUND ->%d\n", ligature->ligatureGlyph);
    glyph_array->array[index] = ligature->ligatureGlyph;
    GlyphArray_put(glyph_array, index + 1, glyph_array, index + ligature->componentCount, glyph_array->len - (index + ligature->componentCount));
    GlyphArray_shrink(glyph_array, ligature->componentCount - 1);
    return true;
  }
  return false;
}


typedef struct packed_BE {
  uint16_t _skip; // Needed because structs can't have only the flexible array member
  uint16_t data[];
} GenericArray;

static bool check_with_Sequence(const GlyphArray *glyph_array, size_t index, GenericArray *sequenceRule, uint16_t sequenceSize, int8_t step) {
  // skip_last is needed because the inputGlyphCount counts also the initial glyph, which is not included in the sequence...
  for (uint16_t i = 0; i < sequenceSize; i++) {
    if (glyph_array->array[index + (i * step)] != sequenceRule->data[i]) {
      return false;
    }
  }
  return true;
}

static bool check_with_Coverage(const GlyphArray *glyph_array, size_t index, const uint8_t *coverageTablesBase, GenericArray *coverageTables, uint16_t coverageSize, int8_t step) {
  for (uint16_t i = 0; i < coverageSize; i++) {
    CoverageTable *coverageTable = (CoverageTable *)(coverageTablesBase + coverageTables->data[i]);
    bool applicable = find_in_coverage(coverageTable, glyph_array->array[index + (i * step)], NULL);
    if (!applicable) return false;
  }
  return true;
}

static bool check_with_Class(const GlyphArray *glyph_array, size_t index, const ClassDefGeneric *classDef, GenericArray *sequenceTable, uint16_t sequence_size, int8_t step) {
  for (uint16_t i = 0; i < sequence_size; i++) {
    uint16_t glyph = glyph_array->array[index + (i * step)];
    uint16_t class;
    find_in_class_array(classDef, glyph, &class);
    if (class != sequenceTable->data[i]) {
      return false;
    }
  }
  return true;
}

static void apply_Lookup_index(const LookupList *lookupList, const LookupTable *lookupTable, GlyphArray* glyph_array, size_t *index);

static bool apply_SequenceSubstitution(const LookupList *lookupList, const GenericSequenceContextFormat *genericSequence, GlyphArray* glyph_array, size_t *index) {
  switch (genericSequence->format) {
    case SequenceContextFormat_1: {
      SequenceContextFormat1 *sequenceContext = (SequenceContextFormat1 *)genericSequence;
      CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + sequenceContext->coverageOffset);
      uint32_t coverage_index;
      bool applicable = find_in_coverage(coverageTable, glyph_array->array[*index], &coverage_index);
      if (!applicable) return false;

      SequenceRuleSet *sequenceRuleSet = (SequenceRuleSet *)((uint8_t *)sequenceContext + sequenceContext->seqRuleSetOffsets[coverage_index]);

      for (uint16_t i = 0; i < sequenceRuleSet->seqRuleCount; i++) {
        const SequenceRule *sequenceRule = (SequenceRule *)((uint8_t *)sequenceRuleSet + sequenceRuleSet->seqRuleOffsets[i]);

        if (*index + sequenceRule->glyphCount > glyph_array->len) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Sequence(glyph_array, *index + 1, (GenericArray *)sequenceRule, sequenceRule->glyphCount - 1, +1)) {
          continue;
        }

        GlyphArray *input_ga = GlyphArray_new(sequenceRule->glyphCount);
        GlyphArray_append(input_ga, &glyph_array->array[*index], sequenceRule->glyphCount);
        const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceRule + (1 + sequenceRule->glyphCount) * sizeof(uint16_t));
        for (uint16_t i = 0; i < sequenceRule->seqLookupCount; i++) {
          const SequenceLookupRecord *sequenceLookupRecord = &seqLookupRecords[i];
          const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
          size_t input_index = sequenceLookupRecord->sequenceIndex;
          apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
        }
        GlyphArray_set(glyph_array, *index + input_ga->len, &glyph_array->array[*index + sequenceRule->glyphCount], glyph_array->len - (*index + sequenceRule->glyphCount));
        GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
        if (input_ga->len < sequenceRule->glyphCount) {
          GlyphArray_shrink(glyph_array, sequenceRule->glyphCount - input_ga->len);
        }
        *index += input_ga->len - 1; // ++ will be done by apply_Lookup
        GlyphArray_free(input_ga);
        return true;
      }
      break;
    }
    case SequenceContextFormat_2: {
      SequenceContextFormat2 *sequenceContext = (SequenceContextFormat2 *)genericSequence;
      CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + sequenceContext->coverageOffset);
      bool applicable = find_in_coverage(coverageTable, glyph_array->array[*index], NULL);
      if (!applicable) return false;

      ClassDefGeneric *inputClassDef = (ClassDefGeneric *)((uint8_t *)sequenceContext + sequenceContext->classDefOffset);

      uint16_t starting_class;
      find_in_class_array(inputClassDef, glyph_array->array[*index], &starting_class);
      if (starting_class >= sequenceContext->classSeqRuleSetCount) {
        // ??
        break;
      }
      const ClassSequenceRuleSet *ruleSet = (ClassSequenceRuleSet *)((uint8_t *)sequenceContext + sequenceContext->classSeqRuleSetOffsets[starting_class]);
      if (ruleSet == NULL) {
        break;
      }

      bool applied = false;
      for (uint16_t i = 0; i < ruleSet->classSeqRuleCount; i++) {
        const ClassSequenceRule *sequenceRule = (ClassSequenceRule *)((uint8_t *)ruleSet + ruleSet->classSeqRuleOffsets[i]);

        if (*index + sequenceRule->glyphCount > glyph_array->len) {
          continue;
        }
        // The sequenceRule doesn't include the initial glyph.
        if(!check_with_Class(glyph_array, *index + 1, inputClassDef, (GenericArray *)((uint8_t *)sequenceRule + 1 * sizeof(uint16_t)), sequenceRule->glyphCount - 1, +1)) {
          continue;
        }

        GlyphArray *input_ga = GlyphArray_new(sequenceRule->glyphCount);
        GlyphArray_append(input_ga, &glyph_array->array[*index], sequenceRule->glyphCount);
        const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceRule + (1 + sequenceRule->glyphCount) * sizeof(uint16_t));
        for (uint16_t i = 0; i < sequenceRule->seqLookupCount; i++) {
          const SequenceLookupRecord *sequenceLookupRecord = &seqLookupRecords[i];
          const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
          size_t input_index = sequenceLookupRecord->sequenceIndex;
          apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
        }
        GlyphArray_set(glyph_array, *index + input_ga->len, &glyph_array->array[*index + sequenceRule->glyphCount], glyph_array->len - (*index + sequenceRule->glyphCount));
        GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
        if (input_ga->len < sequenceRule->glyphCount) {
          GlyphArray_shrink(glyph_array, sequenceRule->glyphCount - input_ga->len);
        }
        *index += input_ga->len - 1; // ++ will be done by apply_Lookup
        GlyphArray_free(input_ga);

        // Only use the first one that matches.
        applied = true;
        break;
      }
      return applied;
    }
    case SequenceContextFormat_3: {
      const SequenceContextFormat3 *sequenceContext = (SequenceContextFormat3 *)genericSequence;

      if (*index + sequenceContext->glyphCount > glyph_array->len) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index, (uint8_t *)genericSequence, (GenericArray *)((uint8_t *)sequenceContext + sizeof(uint16_t) * 2), sequenceContext->glyphCount, +1)) {
        return false;
      }

      GlyphArray *input_ga = GlyphArray_new(sequenceContext->glyphCount);
      GlyphArray_append(input_ga, &glyph_array->array[*index], sequenceContext->glyphCount);
      const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceContext + (2 + sequenceContext->glyphCount + 1) * sizeof(uint16_t));
      for (uint16_t i = 0; i < sequenceContext->seqLookupCount; i++) {
        const SequenceLookupRecord *sequenceLookupRecord = &seqLookupRecords[i];
        const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
        size_t input_index = sequenceLookupRecord->sequenceIndex;
        apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
      }
      GlyphArray_set(glyph_array, *index + input_ga->len, &(glyph_array->array[*index + sequenceContext->glyphCount]), glyph_array->len - (*index + sequenceContext->glyphCount));
      GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
      if (input_ga->len < sequenceContext->glyphCount) {
        GlyphArray_shrink(glyph_array, sequenceContext->glyphCount - input_ga->len);
      }
      *index += input_ga->len - 1; // ++ will be done by apply_Lookup
      GlyphArray_free(input_ga);
      return true;
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN SequenceContextFormat %d\n", genericSequence->format);
      break;
  }
  return false;
}

static bool apply_ChainedSequenceSubstitution(const LookupList *lookupList, const GenericChainedSequenceContextFormat *genericChainedSequence, GlyphArray* glyph_array, size_t *index) {
  switch (genericChainedSequence->format) {
    case ChainedSequenceContextFormat_1: {
      ChainedSequenceContextFormat1 *chainedSequenceContext = (ChainedSequenceContextFormat1 *)genericChainedSequence;
      CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->coverageOffset);
      uint32_t coverage_index;
      bool applicable = find_in_coverage(coverageTable, glyph_array->array[*index], &coverage_index);
      if (!applicable || coverage_index >= chainedSequenceContext->chainedSeqRuleSetCount) return false;

      ChainedSequenceRuleSet *chainedSequenceRuleSet = (ChainedSequenceRuleSet *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->chainedSeqRuleSetOffsets[coverage_index]);

      for (uint16_t i = 0; i < chainedSequenceRuleSet->chainedSeqRuleCount; i++) {
        const ChainedSequenceRule *chainedSequenceRule = (ChainedSequenceRule *)((uint8_t *)chainedSequenceRuleSet + chainedSequenceRuleSet->chainedSeqRuleOffsets[i]);
        const ChainedSequenceRule_backtrack *backtrackSequenceRule = (ChainedSequenceRule_backtrack *)chainedSequenceRule;
        const ChainedSequenceRule_input *inputSequenceRule = (ChainedSequenceRule_input *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * (backtrackSequenceRule->backtrackGlyphCount + 1));
        const ChainedSequenceRule_lookahead *lookaheadSequenceRule = (ChainedSequenceRule_lookahead *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * (inputSequenceRule->inputGlyphCount)); // inputGlyphCount includes the initial one, that's not present in the array
        const ChainedSequenceRule_seq *sequenceRule = (ChainedSequenceRule_seq *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * (lookaheadSequenceRule->lookaheadGlyphCount + 1));

        if (*index + inputSequenceRule->inputGlyphCount + lookaheadSequenceRule->lookaheadGlyphCount > glyph_array->len) {
          continue;
        }
        if (backtrackSequenceRule->backtrackGlyphCount > *index) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Sequence(glyph_array, *index + 1, (GenericArray *)inputSequenceRule, inputSequenceRule->inputGlyphCount - 1, +1)) {
          continue;
        }
        if(!check_with_Sequence(glyph_array, *index - 1, (GenericArray *)backtrackSequenceRule, backtrackSequenceRule->backtrackGlyphCount, -1)) {
          continue;
        }
        if(!check_with_Sequence(glyph_array, *index + inputSequenceRule->inputGlyphCount, (GenericArray *)lookaheadSequenceRule, lookaheadSequenceRule->lookaheadGlyphCount, +1)) {
          continue;
        }

        GlyphArray *input_ga = GlyphArray_new(inputSequenceRule->inputGlyphCount);
        GlyphArray_append(input_ga, &glyph_array->array[*index], inputSequenceRule->inputGlyphCount);
        for (uint16_t i = 0; i < sequenceRule->seqLookupCount; i++) {
          const SequenceLookupRecord *sequenceLookupRecord = &sequenceRule->seqLookupRecords[i];
          const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
          size_t input_index = sequenceLookupRecord->sequenceIndex;
          apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
        }
        GlyphArray_set(glyph_array, *index + input_ga->len, &glyph_array->array[*index + inputSequenceRule->inputGlyphCount], glyph_array->len - (*index + inputSequenceRule->inputGlyphCount));
        GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
        if (input_ga->len < inputSequenceRule->inputGlyphCount) {
          GlyphArray_shrink(glyph_array, inputSequenceRule->inputGlyphCount - input_ga->len);
        }
        *index += input_ga->len - 1; // ++ will be done by apply_Lookup
        GlyphArray_free(input_ga);
        return true;
      }
      break;
    }
    case ChainedSequenceContextFormat_2: {
      ChainedSequenceContextFormat2 *chainedSequenceContext = (ChainedSequenceContextFormat2 *)genericChainedSequence;
      CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->coverageOffset);
      bool applicable = find_in_coverage(coverageTable, glyph_array->array[*index], NULL);
      if (!applicable) return false;

      ClassDefGeneric *inputClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->inputClassDefOffset);
      ClassDefGeneric *backtrackClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->backtrackClassDefOffset);
      ClassDefGeneric *lookaheadClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->lookaheadClassDefOffset);

      uint16_t starting_class;
      find_in_class_array(inputClassDef, glyph_array->array[*index], &starting_class);

      if (starting_class >= chainedSequenceContext->chainedClassSeqRuleSetCount) {
        // ??
        break;
      }
      const ChainedClassSequenceRuleSet *chainedRuleSet = (ChainedClassSequenceRuleSet *)((uint8_t *)chainedSequenceContext + chainedSequenceContext->chainedClassSeqRuleSetOffsets[starting_class]);
      if (chainedRuleSet == NULL) {
        break;
      }

      bool applied = false;
      for (uint16_t i = 0; i < chainedRuleSet->chainedClassSeqRuleCount; i++) {
        const ChainedClassSequenceRule *chainedClassSequenceRule = (ChainedClassSequenceRule *)((uint8_t *)chainedRuleSet + chainedRuleSet->chainedClassSeqRuleOffsets[i]);
        const ChainedClassSequenceRule_backtrack *backtrackSequenceRule = (ChainedClassSequenceRule_backtrack *)chainedClassSequenceRule;
        const ChainedClassSequenceRule_input *inputSequenceRule = (ChainedClassSequenceRule_input *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * (backtrackSequenceRule->backtrackGlyphCount + 1));
        const ChainedClassSequenceRule_lookahead *lookaheadSequenceRule = (ChainedClassSequenceRule_lookahead *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * (inputSequenceRule->inputGlyphCount)); // inputGlyphCount includes the initial one, that's not present in the array
        const ChainedClassSequenceRule_seq *sequenceRule = (ChainedClassSequenceRule_seq *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * (lookaheadSequenceRule->lookaheadGlyphCount + 1));

        if (*index + inputSequenceRule->inputGlyphCount + lookaheadSequenceRule->lookaheadGlyphCount > glyph_array->len) {
          continue;
        }
        if (backtrackSequenceRule->backtrackGlyphCount > *index) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Class(glyph_array, *index + 1, inputClassDef, (GenericArray *)inputSequenceRule, inputSequenceRule->inputGlyphCount - 1, +1)) {
          continue;
        }
        if(!check_with_Class(glyph_array, *index - 1, backtrackClassDef, (GenericArray *)backtrackSequenceRule, backtrackSequenceRule->backtrackGlyphCount, -1)) {
          continue;
        }
        if(!check_with_Class(glyph_array, *index + inputSequenceRule->inputGlyphCount, lookaheadClassDef, (GenericArray *)lookaheadSequenceRule, lookaheadSequenceRule->lookaheadGlyphCount, +1)) {
          continue;
        }

        GlyphArray *input_ga = GlyphArray_new(inputSequenceRule->inputGlyphCount);
        GlyphArray_append(input_ga, &glyph_array->array[*index], inputSequenceRule->inputGlyphCount);
        for (uint16_t i = 0; i < sequenceRule->seqLookupCount; i++) {
          const SequenceLookupRecord *sequenceLookupRecord = &sequenceRule->seqLookupRecords[i];
          const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
          size_t input_index = sequenceLookupRecord->sequenceIndex;
          apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
        }
        GlyphArray_set(glyph_array, *index + input_ga->len, &glyph_array->array[*index + inputSequenceRule->inputGlyphCount], glyph_array->len - (*index + inputSequenceRule->inputGlyphCount));
        GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
        if (input_ga->len < inputSequenceRule->inputGlyphCount) {
          GlyphArray_shrink(glyph_array, inputSequenceRule->inputGlyphCount - input_ga->len);
        }
        *index += input_ga->len - 1; // ++ will be done by apply_Lookup
        GlyphArray_free(input_ga);

        // Only use the first one that matches.
        applied = true;
        break;
      }
      return applied;
    }
    case ChainedSequenceContextFormat_3: {
      const ChainedSequenceContextFormat3_backtrack *backtrackCoverage = (ChainedSequenceContextFormat3_backtrack *)((uint8_t *)genericChainedSequence + sizeof(uint16_t));
      const ChainedSequenceContextFormat3_input *inputCoverage = (ChainedSequenceContextFormat3_input *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * (backtrackCoverage->backtrackGlyphCount + 1));
      const ChainedSequenceContextFormat3_lookahead *lookaheadCoverage = (ChainedSequenceContextFormat3_lookahead *)((uint8_t *)inputCoverage + sizeof(uint16_t) * (inputCoverage->inputGlyphCount + 1));
      const ChainedSequenceContextFormat3_seq *seqCoverage = (ChainedSequenceContextFormat3_seq *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * (lookaheadCoverage->lookaheadGlyphCount + 1));

      if (*index + inputCoverage->inputGlyphCount + lookaheadCoverage->lookaheadGlyphCount > glyph_array->len) {
        return false;
      }
      if (backtrackCoverage->backtrackGlyphCount > *index) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index, (uint8_t *)genericChainedSequence, (GenericArray *)(inputCoverage), inputCoverage->inputGlyphCount, +1)) {
        return false;
      }
      // backtrack is defined with inverse order, so glyph index - 2 will be backtrack coverage index 2
      if (!check_with_Coverage(glyph_array, *index - 1, (uint8_t *)genericChainedSequence, (GenericArray *)(backtrackCoverage), backtrackCoverage->backtrackGlyphCount, -1)) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index + inputCoverage->inputGlyphCount, (uint8_t *)genericChainedSequence, (GenericArray *)(lookaheadCoverage), lookaheadCoverage->lookaheadGlyphCount, +1)) {
        return false;
      }

      if (inputCoverage->inputGlyphCount == 0) return true;

      GlyphArray *input_ga = GlyphArray_new(inputCoverage->inputGlyphCount);
      GlyphArray_append(input_ga, &glyph_array->array[*index], inputCoverage->inputGlyphCount);
      for (uint16_t i = 0; i < seqCoverage->seqLookupCount; i++) {
        const SequenceLookupRecord *sequenceLookupRecord = &seqCoverage->seqLookupRecords[i];
        const LookupTable *lookup = get_lookup(lookupList, sequenceLookupRecord->lookupListIndex);
        size_t input_index = sequenceLookupRecord->sequenceIndex;
        apply_Lookup_index(lookupList, lookup, input_ga, &input_index);
      }
      GlyphArray_set(glyph_array, *index + input_ga->len, &(glyph_array->array[*index + inputCoverage->inputGlyphCount]), glyph_array->len - (*index + inputCoverage->inputGlyphCount));
      GlyphArray_set(glyph_array, *index, input_ga->array, input_ga->len);
      if (input_ga->len < inputCoverage->inputGlyphCount) {
        GlyphArray_shrink(glyph_array, inputCoverage->inputGlyphCount - input_ga->len);
      }
      *index += input_ga->len - 1; // ++ will be done by apply_Lookup
      GlyphArray_free(input_ga);
      return true;
    }
    default:
      fprintf(stderr, "UNKNOWN ChainedSequenceContextFormat %d\n", genericChainedSequence->format);
      break;
  }
  return false;
}


static bool apply_ReverseChainingContextSingleLookupType(const ReverseChainSingleSubstFormat1 *reverseChain, GlyphArray* glyph_array, size_t index) {
  CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)reverseChain + reverseChain->coverageOffset);
  switch (reverseChain->substFormat) {
    case ReverseChainSingleSubstFormat_1: {
      uint32_t coverage_index;
      bool applicable = find_in_coverage(coverageTable, glyph_array->array[index], &coverage_index);
      if (!applicable) return false;

      const ReverseChainSingleSubstFormat1_backtrack *backtrackCoverage = (ReverseChainSingleSubstFormat1_backtrack *)((uint8_t *)reverseChain + sizeof(uint16_t) * 2);
      const ReverseChainSingleSubstFormat1_lookahead *lookaheadCoverage = (ReverseChainSingleSubstFormat1_lookahead *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * (backtrackCoverage->backtrackGlyphCount + 1));
      const ReverseChainSingleSubstFormat1_sub *substitutionTable = (ReverseChainSingleSubstFormat1_sub *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * (lookaheadCoverage->lookaheadGlyphCount + 1));

      if (index + lookaheadCoverage->lookaheadGlyphCount >= glyph_array->len) {
        return false;
      }
      if (backtrackCoverage->backtrackGlyphCount > index) {
        return false;
      }
      // backtrack is defined with inverse order, so glyph index - 2 will be backtrack coverage index 2
      if (!check_with_Coverage(glyph_array, index - 1, (uint8_t *)reverseChain, (GenericArray *)(backtrackCoverage), backtrackCoverage->backtrackGlyphCount, -1)) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, index + 1, (uint8_t *)reverseChain, (GenericArray *)(lookaheadCoverage), lookaheadCoverage->lookaheadGlyphCount, +1)) {
        return false;
      }

      if (coverage_index > substitutionTable->glyphCount) {
        fprintf(stderr, "Possible mistake in ReverseChainingContextSingleLookupType implementation\n");
        return false;
      }
      // TODO: we're using the coverage_index, but this is from our *generated* table, not the original one
      //       so this is potentially very wrong.
      glyph_array->array[index] = substitutionTable->substituteGlyphIDs[coverage_index];
      return true;
    }
    default:
      fprintf(stderr, "UNKNOWN ReverseChainSingleSubstFormat %d\n", reverseChain->substFormat);
      break;
  }
  return false;
}

static bool apply_lookup_subtable(const LookupList *lookupList, GlyphArray* glyph_array, GenericSubstTable *genericSubstTable, uint16_t lookupType, size_t *index) {
  bool applied = false;
  switch (lookupType) {
    case SingleLookupType: {
      SingleSubstFormatGeneric *singleSubstFormatGeneric = (SingleSubstFormatGeneric *)genericSubstTable;
      // Stop at the first one we apply
      applied = apply_SingleSubstitution(singleSubstFormatGeneric, glyph_array, *index);
      break;
    }
    case MultipleLookupType: {
      MultipleSubstFormat1 *multipleSubstFormat = (MultipleSubstFormat1 *)genericSubstTable;
      // Stop at the first one we apply
      applied = apply_MultipleSubstitution(multipleSubstFormat, glyph_array, index);
      break;
    }
    case AlternateLookupType: // TODO: just filter those out at chain creation time...
      // We don't really need to support it.
      // Most use-cases revolve around user selection from the list of alternates,
      // which we don't... really care about.
      // Maybe we could think about enabling this for some weird features like 'rand'.
      fprintf(stderr, "AlternateLookupType unsupported\n");
      break;
    case LigatureLookupType: {
      LigatureSubstitutionTable *ligatureSubstitutionTable = (LigatureSubstitutionTable *)genericSubstTable;
      // Stop at the first one we apply
      applied = apply_LigatureSubstitution(ligatureSubstitutionTable, glyph_array, *index);
      break;
    }
    case ContextLookupType: {
      GenericSequenceContextFormat *genericSequence = (GenericSequenceContextFormat *)genericSubstTable;
      applied = apply_SequenceSubstitution(lookupList, genericSequence, glyph_array, index);
      break;
    }
    case ChainingLookupType: {
      GenericChainedSequenceContextFormat *genericChainedSequence = (GenericChainedSequenceContextFormat *)genericSubstTable;
      applied = apply_ChainedSequenceSubstitution(lookupList, genericChainedSequence, glyph_array, index);
      break;
    }
    case ExtensionSubstitutionLookupType: {
      ExtensionSubstitutionTable *extensionSubstitutionTable = (ExtensionSubstitutionTable *)genericSubstTable;
      GenericSubstTable *_genericSubstTable = (GenericSubstTable *)((uint8_t *)extensionSubstitutionTable + extensionSubstitutionTable->extensionOffset);
      applied = apply_lookup_subtable(lookupList, glyph_array, _genericSubstTable, extensionSubstitutionTable->extensionLookupType, index);
      break;
    }
    case ReverseChainingContextSingleLookupType: {
      ReverseChainSingleSubstFormat1 *reverseChain = (ReverseChainSingleSubstFormat1 *)genericSubstTable;
      // Stop at the first one we apply
      applied = apply_ReverseChainingContextSingleLookupType(reverseChain, glyph_array, (glyph_array->len - *index - 1));
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN LookupType\n");
      break;
  }
  return applied;
}

static void apply_Lookup_index(const LookupList *lookupList, const LookupTable *lookupTable, GlyphArray* glyph_array, size_t *index) {
  // Stop at the first substitution that's successfully applied.
  for (uint16_t i = 0; i < lookupTable->subTableCount; i++) {
    // TODO: we might want to check lookupTable->lookupFlag
    GenericSubstTable *genericSubstTable = (GenericSubstTable *)((uint8_t *)lookupTable + lookupTable->subtableOffsets[i]);
    if (apply_lookup_subtable(lookupList, glyph_array, genericSubstTable, lookupTable->lookupType, index)) {
      break;
    }
  }
}

static void apply_Lookup(const LookupList *lookupList, const LookupTable *lookupTable, GlyphArray* glyph_array) {
  size_t index = 0;
  // For each glyph
  while (index < glyph_array->len) {
    apply_Lookup_index(lookupList, lookupTable, glyph_array, &index);
    index++;
  }
}

GlyphArray *apply_chain(const Chain *chain, const GlyphArray* glyph_array) {
  GlyphArray *ga = GlyphArray_new_from_GlyphArray(glyph_array);
  const GsubHeader *gsubHeader = (GsubHeader *)chain->GSUB_table;
  const LookupList *lookupList = (LookupList *)((uint8_t *)gsubHeader + gsubHeader->lookupListOffset); // Needed for Chained Sequence Context Format
  for (size_t i = 0; i < chain->lookupCount; i++) {
    // printf("Applying lookup %ld\n", i);
    apply_Lookup(lookupList, chain->lookupsArray[i], ga);
  }
  return ga;
}

