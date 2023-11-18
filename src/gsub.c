#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "gsub.h"
#include "bloom.h"
#include "glypharray.h"
#include "bswap.h"
#include "hash.h"

build_hash_functions(Bloom)
build_hash_functions(uintptr_t)

typedef struct LBT_Chain {
  const LookupTable * const *lookupsArray;
  size_t lookupCount;
  const GsubHeader *gsubHeader;
  const LookupList *lookupList;
  HashTable_Bloom *bloom_hash;
  HashTable_uintptr_t *ptr_hash;
} Chain;

#define compare_tags(tag1, tag2) ((tag1)[0] == (tag2)[0] &&                     \
                                  (tag1)[1] == (tag2)[1] &&                     \
                                  (tag1)[2] == (tag2)[2] &&                     \
                                  (tag1)[3] == (tag2)[3])

const unsigned char DFLT_tag[4] = {'D', 'F', 'L', 'T'};
const unsigned char dflt_tag[4] = {'d', 'f', 'l', 't'};
const unsigned char _RQD_tag[4] = {' ', 'R', 'Q', 'D'};
const unsigned char latn_tag[4] = {'l', 'a', 't', 'n'};

typedef enum {
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

typedef enum {
  SingleSubstitutionFormat_1 = 1,
  SingleSubstitutionFormat_2
} SingleSubstitutionFormats;

typedef enum {
  SequenceContextFormat_1 = 1,
  SequenceContextFormat_2,
  SequenceContextFormat_3
} SequenceContextFormats;

typedef enum {
  ChainedSequenceContextFormat_1 = 1,
  ChainedSequenceContextFormat_2,
  ChainedSequenceContextFormat_3
} ChainedSequenceContextFormats;

typedef enum {
  ClassFormat_1 = 1,
  ClassFormat_2
} ClassFormats;

typedef enum {
  ReverseChainSingleSubstFormat_1 = 1,
} ReverseChainSingleSubstFormat;

// Return the ScriptTable with the specified tag.
// If the tag is NULL, the default script is returned.
static const ScriptTable *get_script_table(const ScriptList *scriptList, const unsigned char (*script)[4]) {
  uint16_t scriptCount = parse_16(scriptList->scriptCount);
  for (uint16_t i = 0; i < scriptCount; i++) {
    const ScriptRecord *scriptRecord = &scriptList->scriptRecords[i];
    const ScriptTable *scriptTable = (ScriptTable *)((uint8_t *)scriptList + parse_16(scriptRecord->scriptOffset));
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
static const LangSysTable *get_lang_table(const ScriptTable *scriptTable, const unsigned char (*lang)[4]) {
  // Try to use the default language
  if (lang == NULL || compare_tags(DFLT_tag, *lang) || compare_tags(dflt_tag, *lang)) {
    if (parse_16(scriptTable->defaultLangSysOffset) != 0) {
      return (LangSysTable *)((uint8_t *)scriptTable + parse_16(scriptTable->defaultLangSysOffset));
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

  uint16_t langSysCount = parse_16(scriptTable->langSysCount);
  for (uint16_t i = 0; i < langSysCount; i++) {
    const LangSysRecord *langSysRecord = &scriptTable->langSysRecords[i];
    if (compare_tags(langSysRecord->langSysTag, *lang)) {
      return (LangSysTable *)((uint8_t *)scriptTable + parse_16(langSysRecord->langSysOffset));
    }
  }

  return NULL;
}

// Return the FeatureTable from the FeatureList at the specified index.
// If featureTag is not NULL, it's set to the tag of the feature.
static const FeatureTable *get_feature(const FeatureList *featureList, uint16_t index, unsigned char (*featureTag)[4]) {
  if (index > parse_16(featureList->featureCount)) return NULL;

  const FeatureRecord *featureRecord = &(featureList->featureRecords[index]);
  const FeatureTable *featureTable = (FeatureTable *)((uint8_t *)featureList + parse_16(featureRecord->featureOffset));
  if (featureTag != NULL) {
    (*featureTag)[0] = featureRecord->featureTag[0];
    (*featureTag)[1] = featureRecord->featureTag[1];
    (*featureTag)[2] = featureRecord->featureTag[2];
    (*featureTag)[3] = featureRecord->featureTag[3];
  }
  return featureTable;
}

static LookupTable *get_lookup(const LookupList *lookupList, uint16_t index) {
  if (index > parse_16(lookupList->lookupCount)) return NULL;
  return (LookupTable *)((uint8_t *)lookupList + parse_16(lookupList->lookupOffsets[index]));
}


// Returns the number of lookups inside the FeatureTable.
// lookups must either be an array big enough to contain
// all the pointers to lookups, or be NULL.
static size_t get_lookups_from_feature(const FeatureTable *featureTable, const LookupList *lookupList, bool *lookups_map) {
  size_t c = 0;
  uint16_t lookupIndexCount = parse_16(featureTable->lookupIndexCount);
  for (uint16_t k = 0; k < lookupIndexCount; k++) {
    uint16_t lookup_index = parse_16(featureTable->lookupListIndices[k]);
    LookupTable *lookup = get_lookup(lookupList, lookup_index);
    if (lookup == NULL) {
      fprintf(stderr, "Warning: unable to obtain lookup of feature\n");
      // Try with next feature
      continue;
    }
    if (!lookups_map[lookup_index]) {
      lookups_map[lookup_index] = true;
      c++;
    }
  }
  return c;
}

// Returns the number of lookups of the given LangSysTable,
// filtered and sorted as specified in features_enabled.
// lookups must either be an array big enough to contain
// all the pointers to lookups, or be NULL.
static size_t get_lookups(const LangSysTable* langSysTable, const FeatureList *featureList, const LookupList *lookupList, const unsigned char (*features_enabled)[4], size_t nFeatures, LookupTable ***lookups) {
  bool *lookups_map = calloc(parse_16(lookupList->lookupCount), sizeof(bool));
  if (lookups_map == NULL) return 0;

  size_t c = 0;
  if (features_enabled == NULL) goto end;

  uint16_t requiredFeatureIndex = parse_16(langSysTable->requiredFeatureIndex);
  for (size_t i = 0; i < nFeatures; i++) {
    if (requiredFeatureIndex != 0xFFFF && compare_tags(_RQD_tag, features_enabled[i])) {
      const FeatureTable *featureTable = get_feature(featureList, requiredFeatureIndex, NULL);
      c += get_lookups_from_feature(featureTable, lookupList, lookups_map);
      continue;
    }
    uint16_t featureIndexCount = parse_16(langSysTable->featureIndexCount);
    for (uint16_t j = 0; j < featureIndexCount; j++) {
      unsigned char featureTag[4];
      uint16_t index = parse_16(langSysTable->featureIndices[j]);
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
    uint16_t lookupCount = parse_16(lookupList->lookupCount);
    for (uint16_t i = 0, j = 0; i < lookupCount; i++) {
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
Chain *generate_chain(const uint8_t *GSUB_table, const unsigned char (*script)[4], const unsigned char (*lang)[4], const unsigned char (*features)[4], size_t n_features) {
  LookupTable **lookupsArray = NULL;

  if (GSUB_table == NULL) {
    // There is no GSUB table, so return an empty chain
    return calloc(1, sizeof(Chain));
  }

  const GsubHeader *gsubHeader = (GsubHeader *)GSUB_table;

  const ScriptList *scriptList = (ScriptList *)((uint8_t *)gsubHeader + parse_16(gsubHeader->scriptListOffset));
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

  const FeatureList *featureList = (FeatureList *)((uint8_t *)gsubHeader + parse_16(gsubHeader->featureListOffset));
  const LookupList *lookupList = (LookupList *)((uint8_t *)gsubHeader + parse_16(gsubHeader->lookupListOffset));
  ///printf("Total of %d lookups\n", parse_16(lookupList->lookupCount));

  size_t lookupCount = get_lookups(langSysTable, featureList, lookupList, features, n_features, &lookupsArray);

  Chain *chain = NULL;
  chain = calloc(1, sizeof(Chain));
  if (chain == NULL)
    goto fail;

  chain->lookupsArray = (const LookupTable * const *)lookupsArray;
  chain->lookupCount = lookupCount;
  chain->gsubHeader = gsubHeader;
  chain->lookupList = lookupList;
  chain->bloom_hash = new_Bloom_hash();
  if (chain->bloom_hash == NULL)
    goto fail_chain;
  chain->ptr_hash = new_uintptr_t_hash();
  if (chain->ptr_hash == NULL)
    goto fail_chain;

  return chain;

fail:
  // free(GSUB_table);
  free(lookupsArray);
  return NULL;

fail_chain:
  destroy_chain(chain);
  return NULL;
}

void destroy_chain(Chain *chain) {
  if (chain == NULL) return;
  // free((void *)chain->gsubHeader);
  free((void *)chain->lookupsArray);
  free_Bloom_hash(chain->bloom_hash);
  // uintptr_t_hash has malloc'd stuff inside, so free that first
  if (chain->ptr_hash != NULL) {
    for (size_t i = 0; i < chain->ptr_hash->size; i++) {
      if (chain->ptr_hash->entries[i].address != NULL) {
        free((void *)chain->ptr_hash->entries[i].value);
      }
    }
  }
  free_uintptr_t_hash(chain->ptr_hash);
  free(chain);
}

// Returns whether the specified script and language combo has a required feature.
// `script` and `lang` can be NULL to select the default ones.
// Writes in `required_feature` the tag.
bool get_required_feature(const uint8_t *GSUB_table, const unsigned char (*script)[4], const unsigned char (*lang)[4], unsigned char (*required_feature)[4]) {
  bool result = false;

  if (GSUB_table == NULL) {
    goto end;
  }

  const GsubHeader *gsubHeader = (GsubHeader *)GSUB_table;
  const ScriptList *scriptList = (ScriptList *)((uint8_t *)gsubHeader + parse_16(gsubHeader->scriptListOffset));
  const ScriptTable *scriptTable = get_script_table(scriptList, script);
  if (scriptTable == NULL) {
    goto end;
  }
  const LangSysTable *langSysTable = get_lang_table(scriptTable, lang);
  if (langSysTable == NULL) {
    goto end;
  }

  // 0xFFFF means no required feature.
  if (parse_16(langSysTable->requiredFeatureIndex) != 0xFFFF) {
    const FeatureList *featureList = (FeatureList *)((uint8_t *)gsubHeader + parse_16(gsubHeader->featureListOffset));
    const FeatureTable *featureTable = get_feature(featureList, parse_16(langSysTable->requiredFeatureIndex), required_feature);
    if (featureTable != NULL) {
      result = true;
    }
  }

end:
  // free(GSUB_table);
  return result;
}

// Returns the bloom digest that matches with the glyphs for the Coverage.
static Bloom get_Coverage_bloom(const CoverageTable *coverageTable) {
  Bloom bloom = null_bloom;
  switch (parse_16(coverageTable->coverageFormat)) {
    case 1: { // Individual glyph indices
      const CoverageArrayTable *arrayTable = (CoverageArrayTable *)coverageTable;
      uint16_t glyphCount = parse_16(arrayTable->glyphCount);
      if (glyphCount == 0) break;

      for (uint16_t i = 0; i < glyphCount; i++) {
        uint16_t coverageID = parse_16(arrayTable->glyphArray[i]);
        bloom = add_glyphID_to_bloom(bloom, coverageID);
        if (is_full_bloom(bloom)) break;
      }
      break;
    }
    case 2: { // Range of glyphs
      const CoverageRangesTable *rangesTable = (CoverageRangesTable *)coverageTable;
      uint16_t rangeCount = parse_16(rangesTable->rangeCount);
      if (rangeCount == 0) break;
      for (uint16_t i = 0; i < rangeCount; i++) {
        const CoverageRangeRecordTable *range = &rangesTable->rangeRecords[i];
        uint16_t startGlyphID = parse_16(range->startGlyphID);
        uint16_t endGlyphID = parse_16(range->endGlyphID);
        Bloom range_bloom = get_glyphID_range_bloom(startGlyphID, endGlyphID);
        bloom = add_bloom_to_bloom(bloom, range_bloom);
        if (is_full_bloom(bloom)) break;
      }
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN coverage format\n");
      return full_bloom;
  }
  return bloom;
}

// Returns the bloom digest that matches with the glyphs for all the Coverages.
static inline Bloom get_Coverage_array_bloom(const uint8_t *coverageTablesBase, uint16_t *coverageTables, uint16_t coverageSize) {
  Bloom bloom = null_bloom;
  for (uint16_t i = 0; i < coverageSize; i++) {
    const CoverageTable *coverageTable = (CoverageTable *)(coverageTablesBase + parse_16(coverageTables[i]));
    Bloom coverage_bloom = get_Coverage_bloom((const void *)coverageTable);
    bloom = add_bloom_to_bloom(bloom, coverage_bloom);
    if (is_full_bloom(bloom)) break;
  }
  return bloom;
}

static bool find_in_Coverage(const CoverageTable *coverageTable, uint16_t id, uint32_t *index) {
  switch (parse_16(coverageTable->coverageFormat)) {
    case 1: { // Individual glyph indices
      const CoverageArrayTable *arrayTable = (CoverageArrayTable *)coverageTable;
      uint16_t glyphCount = parse_16(arrayTable->glyphCount);
      if (glyphCount == 0) return false;
      uint16_t first_ID = parse_16(arrayTable->glyphArray[0]);
      if (first_ID > id) return false;
      uint16_t last_ID = parse_16(arrayTable->glyphArray[glyphCount - 1]);
      if (last_ID < id) return false;

      // Binary search because we can
      uint16_t top = glyphCount - 1;
      uint16_t bottom = 0;
      while (top - bottom >= 0) {
        uint16_t current = (top + bottom) / 2;
        uint16_t coverageID = parse_16(arrayTable->glyphArray[current]);
        if (id < coverageID) {
          top = current - 1;
        } else if (id > coverageID) {
          bottom = current + 1;
        } else {
          if (index != NULL) {
            *index = current;
          }
          return true;
        }
      }
      break;
    }
    case 2: { // Range of glyphs
      const CoverageRangesTable *rangesTable = (CoverageRangesTable *)coverageTable;
      uint16_t rangeCount = parse_16(rangesTable->rangeCount);
      if (rangeCount == 0) return false;
      const CoverageRangeRecordTable *first_range = &(rangesTable->rangeRecords[0]);
      if (parse_16(first_range->startGlyphID) > id) return false;
      const CoverageRangeRecordTable *last_range = &(rangesTable->rangeRecords[rangeCount - 1]);
      if (parse_16(last_range->endGlyphID) < id) return false;
      uint16_t k = 0;
      for (uint16_t i = 0; i < rangeCount; i++) {
        const CoverageRangeRecordTable *range = &rangesTable->rangeRecords[i];
        uint16_t startGlyphID = parse_16(range->startGlyphID);
        if (id < startGlyphID) break;
        uint16_t endGlyphID = parse_16(range->endGlyphID);
        if (id > endGlyphID) {
          k += endGlyphID - startGlyphID + 1;
          continue;
        }
        if (index != NULL) {
          *index = k + (id - startGlyphID);
        }
        return true;
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
  switch (parse_16(classDefTable->classFormat)) {
    case ClassFormat_1: { // Individual glyph indices
      const ClassDefFormat1 *arrayTable = (ClassDefFormat1 *)classDefTable;
      uint16_t startGlyphID = parse_16(arrayTable->startGlyphID);
      uint16_t glyphCount = parse_16(arrayTable->glyphCount);
      if (id < startGlyphID || id >= startGlyphID + glyphCount) {
        break;
      }
      if (class != NULL) {
        *class = parse_16(arrayTable->classValueArray[id - startGlyphID]);
      }
      return true;
    }
    case ClassFormat_2: { // Range of glyphs
      const ClassDefFormat2 *rangesTable = (ClassDefFormat2 *)classDefTable;
      uint16_t classRangeCount = parse_16(rangesTable->classRangeCount);
      if (classRangeCount == 0) break;
      const ClassRangeRecord *first_range = &(rangesTable->classRangeRecords[0]);
      if (parse_16(first_range->startGlyphID) > id) break;
      const ClassRangeRecord *last_range = &(rangesTable->classRangeRecords[classRangeCount - 1]);
      if (parse_16(last_range->endGlyphID) < id) break;

      // Binary search because we can
      uint16_t top = classRangeCount - 1;
      uint16_t bottom = 0;
      while (top - bottom >= 0) {
        uint16_t current = (top + bottom) / 2;
        const ClassRangeRecord *range = &rangesTable->classRangeRecords[current];
        if (id < parse_16(range->startGlyphID)) {
          top = current - 1;
        } else if (id > parse_16(range->endGlyphID)) {
          bottom = current + 1;
        } else {
          if (class != NULL) {
            *class = parse_16(range->_class);
          }
          return true;
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

static bool apply_SingleSubstitution(const SingleSubstFormatGeneric *singleSubstFormatGeneric, GlyphArray* glyph_array, size_t index) {
  const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)singleSubstFormatGeneric + parse_16(singleSubstFormatGeneric->coverageOffset));
  switch (parse_16(singleSubstFormatGeneric->substFormat)) {
    case SingleSubstitutionFormat_1: {
      const SingleSubstFormat1 *singleSubst = (SingleSubstFormat1 *)singleSubstFormatGeneric;
      if (find_in_Coverage(coverageTable, glyph_array->array[index], NULL)) {
        GlyphArray_set1(glyph_array, index, glyph_array->array[index] + parse_16(singleSubst->deltaGlyphID));
        return true;
      }
      break;
    }
    case SingleSubstitutionFormat_2: {
      const SingleSubstFormat2 *singleSubst = (SingleSubstFormat2 *)singleSubstFormatGeneric;
      uint32_t coverage_index;
      if (find_in_Coverage(coverageTable, glyph_array->array[index], &coverage_index)) {
        GlyphArray_set1(glyph_array, index, parse_16(singleSubst->substituteGlyphIDs[coverage_index]));
        return true;
      }
      break;
    }
    default:
      fprintf(stderr, "UNKNOWN SubstFormat %d\n", parse_16(singleSubstFormatGeneric->substFormat));
      break;
  }
  return false;
}

static bool apply_MultipleSubstitution(const MultipleSubstFormat1 *multipleSubstFormat, GlyphArray* glyph_array, size_t *index) {
  const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)multipleSubstFormat + parse_16(multipleSubstFormat->coverageOffset));
  uint32_t coverage_index;
  bool applicable = find_in_Coverage(coverageTable, glyph_array->array[*index], &coverage_index);
  if (!applicable || coverage_index >= parse_16(multipleSubstFormat->sequenceCount)) return false;

  const SequenceTable *sequenceTable = (SequenceTable *)((uint8_t *)multipleSubstFormat + parse_16(multipleSubstFormat->sequenceOffsets[coverage_index]));
  uint16_t glyphCount = parse_16(sequenceTable->glyphCount);
  GlyphArray_put(glyph_array, *index + glyphCount - 1, glyph_array, *index, glyph_array->len - *index - 1);
  for (uint16_t j = 0; j < glyphCount; j++) {
    GlyphArray_set1(glyph_array, *index + j, parse_16(sequenceTable->substituteGlyphIDs[j]));
  }
  *index += glyphCount - 1; // ++ will be done by apply_Lookup
  return true;
}

static const LigatureTable *find_Ligature(const LigatureSetTable *ligatureSet, GlyphArray* glyph_array, size_t index) {
  uint16_t ligatureCount = parse_16(ligatureSet->ligatureCount);
  for (uint16_t i = 0; i < ligatureCount; i++) {
    const LigatureTable *ligature = (LigatureTable *)((uint8_t *)ligatureSet + parse_16(ligatureSet->ligatureOffsets[i]));
    bool fail = false;
    uint16_t componentCount = parse_16(ligature->componentCount);
    if (index + componentCount - 1 > glyph_array->len) continue;
    for (uint16_t j = 0; j < componentCount - 1; j++) {
      if (parse_16(ligature->componentGlyphIDs[j]) != glyph_array->array[index+j]) {
        fail = true;
        break;
      }
    }
    if (!fail) {
      return ligature;
    }
  }
  return NULL;
}

static bool apply_LigatureSubstitution(const LigatureSubstitutionTable *ligatureSubstitutionTable, GlyphArray* glyph_array, size_t index) {
  const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)ligatureSubstitutionTable + parse_16(ligatureSubstitutionTable->coverageOffset));
  uint32_t coverage_index;
  bool applicable = find_in_Coverage(coverageTable, glyph_array->array[index], &coverage_index);
  if (!applicable || coverage_index >= parse_16(ligatureSubstitutionTable->ligatureSetCount)) return false;
  const LigatureSetTable *ligatureSet = (LigatureSetTable *)((uint8_t *)ligatureSubstitutionTable + parse_16(ligatureSubstitutionTable->ligatureSetOffsets[coverage_index]));
  const LigatureTable *ligature = find_Ligature(ligatureSet, glyph_array, index + 1);
  if (ligature != NULL) {
    GlyphArray_set1(glyph_array, index, parse_16(ligature->ligatureGlyph));
    uint16_t componentCount = parse_16(ligature->componentCount);
    GlyphArray_put(glyph_array, index + 1, glyph_array, index + componentCount, glyph_array->len - (index + componentCount));
    GlyphArray_shrink(glyph_array, componentCount - 1);
    return true;
  }
  return false;
}

static bool check_with_Sequence(GlyphArray *glyph_array, size_t index, const uint16_t *sequenceRule, uint16_t sequenceSize, int8_t step) {
  if (sequenceSize == 0) return true;
  for (uint16_t i = 0; i < sequenceSize; i++) {
    if (glyph_array->array[index + (i * step)] != parse_16(sequenceRule[i])) {
      return false;
    }
  }
  return true;
}

static bool check_with_Coverage(const GlyphArray *glyph_array, size_t index, const uint8_t *coverageTablesBase, const uint16_t *coverageTables, uint16_t coverageSize, int8_t step) {
  if (coverageSize == 0) return true;

  for (uint16_t i = 0; i < coverageSize; i++) {
    const CoverageTable *coverageTable = (CoverageTable *)(coverageTablesBase + parse_16(coverageTables[i]));
    if (!find_in_Coverage(coverageTable, glyph_array->array[index + (i * step)], NULL))
      return false;
  }
  return true;
}

static bool check_with_Class(const GlyphArray *glyph_array, size_t index, const ClassDefGeneric *classDef, const uint16_t *sequenceTable, uint16_t sequence_size, int8_t step) {
  for (uint16_t i = 0; i < sequence_size; i++) {
    uint16_t glyph = glyph_array->array[index + (i * step)];
    uint16_t class;
    find_in_class_array(classDef, glyph, &class);
    if (class != parse_16(sequenceTable[i])) {
      return false;
    }
  }
  return true;
}

static void apply_Lookup_at_index(const Chain *chain, const LookupTable *lookupTable, const Bloom* blooms, GlyphArray* glyph_array, size_t *index);

static void apply_SequenceRule(const Chain *chain, uint16_t glyphCount, const SequenceLookupRecord *seqLookupRecords, uint16_t seqLookupCount, GlyphArray *glyph_array, size_t *index) {
  GlyphArray *input_ga = GlyphArray_new(glyphCount);
  if (input_ga == NULL)
    return; // TODO: panic? We could even just have one per chain and reuse it.
  GlyphArray_append(input_ga, &glyph_array->array[*index], glyphCount);
  for (uint16_t i = 0; i < seqLookupCount; i++) {
    const SequenceLookupRecord *sequenceLookupRecord = &seqLookupRecords[i];
    const LookupTable *lookupTable = get_lookup(chain->lookupList, parse_16(sequenceLookupRecord->lookupListIndex));
    size_t input_index = parse_16(sequenceLookupRecord->sequenceIndex);
    apply_Lookup_at_index(chain, lookupTable, NULL, input_ga, &input_index);
  }
  GlyphArray_put(glyph_array, *index + input_ga->len, glyph_array, *index + glyphCount, glyph_array->len - (*index + glyphCount));
  GlyphArray_put(glyph_array, *index, input_ga, 0, input_ga->len);
  if (input_ga->len < glyphCount) {
    GlyphArray_shrink(glyph_array, glyphCount - input_ga->len);
  }
  *index += input_ga->len - 1; // ++ will be done by apply_Lookup
  GlyphArray_free(input_ga);
}

// Returns the bloom digest that matches with the glyphs that "start" a SequenceSubstitution.
static Bloom get_SequenceSubstitution_bloom(const GenericSequenceContextFormat *genericSequence) {
  switch (parse_16(genericSequence->format)) {
    case SequenceContextFormat_1: {
      const SequenceContextFormat1 *sequenceContext = (SequenceContextFormat1 *)genericSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + parse_16(sequenceContext->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    case SequenceContextFormat_2: {
      const SequenceContextFormat2 *sequenceContext = (SequenceContextFormat2 *)genericSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + parse_16(sequenceContext->coverageOffset));
      // TODO: I'm not sure we can do much for ClassSequences
      return get_Coverage_bloom(coverageTable);
    }
    case SequenceContextFormat_3: {
      const SequenceContextFormat3 *sequenceContext = (SequenceContextFormat3 *)genericSequence;
      uint16_t glyphCount = parse_16(sequenceContext->glyphCount);
      if (glyphCount == 0) return full_bloom;
      return get_Coverage_array_bloom((uint8_t *)genericSequence, (uint16_t *)((uint8_t *)sequenceContext + sizeof(uint16_t) * 3), glyphCount);
    }
    default:
      fprintf(stderr, "UNKNOWN SequenceContextFormat %d\n", parse_16(genericSequence->format));
      return full_bloom;
  }
}

static bool apply_SequenceSubstitution(const Chain *chain, const GenericSequenceContextFormat *genericSequence, GlyphArray* glyph_array, size_t *index) {
  switch (parse_16(genericSequence->format)) {
    case SequenceContextFormat_1: {
      const SequenceContextFormat1 *sequenceContext = (SequenceContextFormat1 *)genericSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + parse_16(sequenceContext->coverageOffset));
      uint32_t coverage_index;
      bool applicable = find_in_Coverage(coverageTable, glyph_array->array[*index], &coverage_index);
      if (!applicable || coverage_index >= parse_16(sequenceContext->seqRuleSetCount)) return false;

      const SequenceRuleSet *sequenceRuleSet = (SequenceRuleSet *)((uint8_t *)sequenceContext + parse_16(sequenceContext->seqRuleSetOffsets[coverage_index]));
      uint16_t seqRuleCount = parse_16(sequenceRuleSet->seqRuleCount);
      for (uint16_t i = 0; i < seqRuleCount; i++) {
        const SequenceRule *sequenceRule = (SequenceRule *)((uint8_t *)sequenceRuleSet + parse_16(sequenceRuleSet->seqRuleOffsets[i]));
        uint16_t sequenceGlyphCount = parse_16(sequenceRule->glyphCount);

        if (*index + sequenceGlyphCount > glyph_array->len) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Sequence(glyph_array, *index + 1, (uint16_t *)((uint8_t *)sequenceRule + sizeof(uint16_t) * 2), sequenceGlyphCount - 1, +1)) {
          continue;
        }
        const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceRule + (1 + sequenceGlyphCount) * sizeof(uint16_t));
        apply_SequenceRule(chain, sequenceGlyphCount, seqLookupRecords, parse_16(sequenceRule->seqLookupCount), glyph_array, index);
        return true;
      }
      break;
    }
    case SequenceContextFormat_2: {
      const SequenceContextFormat2 *sequenceContext = (SequenceContextFormat2 *)genericSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)sequenceContext + parse_16(sequenceContext->coverageOffset));
      if (!find_in_Coverage(coverageTable, glyph_array->array[*index], NULL))
        return false;

      const ClassDefGeneric *inputClassDef = (ClassDefGeneric *)((uint8_t *)sequenceContext + parse_16(sequenceContext->classDefOffset));

      uint16_t starting_class;
      find_in_class_array(inputClassDef, glyph_array->array[*index], &starting_class);
      if (starting_class >= parse_16(sequenceContext->classSeqRuleSetCount)) {
        // ??
        break;
      }
      uint16_t starting_class_offset = parse_16(sequenceContext->classSeqRuleSetOffsets[starting_class]);
      if (starting_class_offset == 0) {
        break;
      }
      const ClassSequenceRuleSet *ruleSet = (ClassSequenceRuleSet *)((uint8_t *)sequenceContext + starting_class_offset);
      uint16_t classSeqRuleCount = parse_16(ruleSet->classSeqRuleCount);
      for (uint16_t i = 0; i < classSeqRuleCount; i++) {
        const ClassSequenceRule *sequenceRule = (ClassSequenceRule *)((uint8_t *)ruleSet + parse_16(ruleSet->classSeqRuleOffsets[i]));
        uint16_t sequenceGlyphCount = parse_16(sequenceRule->glyphCount);

        if (*index + sequenceGlyphCount > glyph_array->len) {
          continue;
        }
        // The sequenceRule doesn't include the initial glyph.
        if(!check_with_Class(glyph_array, *index + 1, inputClassDef, (uint16_t *)((uint8_t *)sequenceRule + 2 * sizeof(uint16_t)), sequenceGlyphCount - 1, +1)) {
          continue;
        }

        const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceRule + (1 + sequenceGlyphCount) * sizeof(uint16_t));
        apply_SequenceRule(chain, sequenceGlyphCount, seqLookupRecords, parse_16(sequenceRule->seqLookupCount), glyph_array, index);
        // Only use the first one that matches.
        return true;
      }
      break;
    }
    case SequenceContextFormat_3: {
      const SequenceContextFormat3 *sequenceContext = (SequenceContextFormat3 *)genericSequence;
      uint16_t glyphCount = parse_16(sequenceContext->glyphCount);

      if (*index + glyphCount > glyph_array->len) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index, (uint8_t *)genericSequence, (uint16_t *)((uint8_t *)sequenceContext + sizeof(uint16_t) * 3), glyphCount, +1)) {
        return false;
      }

      const SequenceLookupRecord *seqLookupRecords = (SequenceLookupRecord *)((uint8_t *)sequenceContext + (2 + glyphCount + 1) * sizeof(uint16_t));
      apply_SequenceRule(chain, glyphCount, seqLookupRecords, parse_16(sequenceContext->seqLookupCount), glyph_array, index);
      return true;
    }
    default:
      fprintf(stderr, "UNKNOWN SequenceContextFormat %d\n", parse_16(genericSequence->format));
      break;
  }
  return false;
}

// Returns the bloom digest that matches with the glyphs that "start" a ChainedSequenceSubstitution.
static Bloom get_ChainedSequenceSubstitution_bloom(const GenericChainedSequenceContextFormat *genericChainedSequence) {
  switch (parse_16(genericChainedSequence->format)) {
    case ChainedSequenceContextFormat_1: {
      const ChainedSequenceContextFormat1 *chainedSequenceContext = (ChainedSequenceContextFormat1 *)genericChainedSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    case ChainedSequenceContextFormat_2: {
      const ChainedSequenceContextFormat2 *chainedSequenceContext = (ChainedSequenceContextFormat2 *)genericChainedSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->coverageOffset));
      // TODO: I'm not sure we can do much for ClassSequences
      return get_Coverage_bloom(coverageTable);
    }
    case ChainedSequenceContextFormat_3: {
      const ChainedSequenceContextFormat3_backtrack *backtrackCoverage = (ChainedSequenceContextFormat3_backtrack *)((uint8_t *)genericChainedSequence + sizeof(uint16_t));
      uint16_t backtrackGlyphCount = parse_16(backtrackCoverage->backtrackGlyphCount);
      const ChainedSequenceContextFormat3_input *inputCoverage = (ChainedSequenceContextFormat3_input *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * (backtrackGlyphCount + 1));
      uint16_t inputGlyphCount = parse_16(inputCoverage->inputGlyphCount);

      if (inputGlyphCount == 0) return full_bloom;

      return get_Coverage_array_bloom((uint8_t *)genericChainedSequence, (uint16_t *)((uint8_t *)inputCoverage + sizeof(uint16_t) * 1), inputGlyphCount);
    }
    default:
      fprintf(stderr, "UNKNOWN ChainedSequenceContextFormat %d\n", parse_16(genericChainedSequence->format));
      return full_bloom;
  }
}

static bool apply_ChainedSequenceSubstitution(const Chain *chain, const GenericChainedSequenceContextFormat *genericChainedSequence, GlyphArray* glyph_array, size_t *index) {
  switch (parse_16(genericChainedSequence->format)) {
    case ChainedSequenceContextFormat_1: {
      const ChainedSequenceContextFormat1 *chainedSequenceContext = (ChainedSequenceContextFormat1 *)genericChainedSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->coverageOffset));
      uint32_t coverage_index;
      bool applicable = find_in_Coverage(coverageTable, glyph_array->array[*index], &coverage_index);
      if (!applicable || coverage_index >= parse_16(chainedSequenceContext->chainedSeqRuleSetCount)) return false;

      const ChainedSequenceRuleSet *chainedSequenceRuleSet = (ChainedSequenceRuleSet *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->chainedSeqRuleSetOffsets[coverage_index]));
      uint16_t chainedSeqRuleCount = parse_16(chainedSequenceRuleSet->chainedSeqRuleCount);
      for (uint16_t i = 0; i < chainedSeqRuleCount; i++) {
        const ChainedSequenceRule *chainedSequenceRule = (ChainedSequenceRule *)((uint8_t *)chainedSequenceRuleSet + parse_16(chainedSequenceRuleSet->chainedSeqRuleOffsets[i]));
        const ChainedSequenceRule_backtrack *backtrackSequenceRule = (ChainedSequenceRule_backtrack *)chainedSequenceRule;
        uint16_t backtrackGlyphCount = parse_16(backtrackSequenceRule->backtrackGlyphCount);
        const ChainedSequenceRule_input *inputSequenceRule = (ChainedSequenceRule_input *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * (backtrackGlyphCount + 1));
        uint16_t inputGlyphCount = parse_16(inputSequenceRule->inputGlyphCount);
        // inputGlyphCount includes the initial one, that's not present in the array, so no need to +1 here
        const ChainedSequenceRule_lookahead *lookaheadSequenceRule = (ChainedSequenceRule_lookahead *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * (inputGlyphCount));
        uint16_t lookaheadGlyphCount = parse_16(lookaheadSequenceRule->lookaheadGlyphCount);
        const ChainedSequenceRule_seq *sequenceRule = (ChainedSequenceRule_seq *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * (lookaheadGlyphCount + 1));
        uint16_t seqLookupCount = parse_16(sequenceRule->seqLookupCount);

        if (*index + inputGlyphCount + lookaheadGlyphCount > glyph_array->len) {
          continue;
        }
        if (backtrackGlyphCount > *index) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Sequence(glyph_array, *index + 1, (uint16_t *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * 1), inputGlyphCount - 1, +1)) {
          continue;
        }
        if(!check_with_Sequence(glyph_array, *index - 1, (uint16_t *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * 1), backtrackGlyphCount, -1)) {
          continue;
        }
        if(!check_with_Sequence(glyph_array, *index + inputGlyphCount, (uint16_t *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * 1), lookaheadGlyphCount, +1)) {
          continue;
        }

        apply_SequenceRule(chain, inputGlyphCount, sequenceRule->seqLookupRecords, seqLookupCount, glyph_array, index);
        return true;
      }
      break;
    }
    case ChainedSequenceContextFormat_2: {
      const ChainedSequenceContextFormat2 *chainedSequenceContext = (ChainedSequenceContextFormat2 *)genericChainedSequence;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->coverageOffset));
      bool applicable = find_in_Coverage(coverageTable, glyph_array->array[*index], NULL);
      if (!applicable) return false;

      const ClassDefGeneric *inputClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->inputClassDefOffset));
      const ClassDefGeneric *backtrackClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->backtrackClassDefOffset));
      const ClassDefGeneric *lookaheadClassDef = (ClassDefGeneric *)((uint8_t *)chainedSequenceContext + parse_16(chainedSequenceContext->lookaheadClassDefOffset));

      uint16_t starting_class;
      find_in_class_array(inputClassDef, glyph_array->array[*index], &starting_class);

      if (starting_class >= parse_16(chainedSequenceContext->chainedClassSeqRuleSetCount)) {
        // ??
        break;
      }
      uint16_t starting_class_offset = parse_16(chainedSequenceContext->chainedClassSeqRuleSetOffsets[starting_class]);
      if (starting_class_offset == 0) {
        break;
      }
      const ChainedClassSequenceRuleSet *chainedRuleSet = (ChainedClassSequenceRuleSet *)((uint8_t *)chainedSequenceContext + starting_class_offset);
      uint16_t chainedClassSeqRuleCount = parse_16(chainedRuleSet->chainedClassSeqRuleCount);
      for (uint16_t i = 0; i < chainedClassSeqRuleCount; i++) {
        const ChainedClassSequenceRule *chainedClassSequenceRule = (ChainedClassSequenceRule *)((uint8_t *)chainedRuleSet + parse_16(chainedRuleSet->chainedClassSeqRuleOffsets[i]));
        const ChainedClassSequenceRule_backtrack *backtrackSequenceRule = (ChainedClassSequenceRule_backtrack *)chainedClassSequenceRule;
        uint16_t backtrackGlyphCount = parse_16(backtrackSequenceRule->backtrackGlyphCount);
        const ChainedClassSequenceRule_input *inputSequenceRule = (ChainedClassSequenceRule_input *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * (backtrackGlyphCount + 1));
        uint16_t inputGlyphCount = parse_16(inputSequenceRule->inputGlyphCount);
        const ChainedClassSequenceRule_lookahead *lookaheadSequenceRule = (ChainedClassSequenceRule_lookahead *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * (inputGlyphCount)); // inputGlyphCount includes the initial one, that's not present in the array
        uint16_t lookaheadGlyphCount = parse_16(lookaheadSequenceRule->lookaheadGlyphCount);
        const ChainedClassSequenceRule_seq *sequenceRule = (ChainedClassSequenceRule_seq *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * (lookaheadGlyphCount + 1));
        uint16_t seqLookupCount = parse_16(sequenceRule->seqLookupCount);

        if (*index + inputGlyphCount + lookaheadGlyphCount > glyph_array->len) {
          continue;
        }
        if (backtrackGlyphCount > *index) {
          continue;
        }
        // The inputSequence doesn't include the initial glyph.
        if(!check_with_Class(glyph_array, *index + 1, inputClassDef, (uint16_t *)((uint8_t *)inputSequenceRule + sizeof(uint16_t) * 1), inputGlyphCount - 1, +1)) {
          continue;
        }
        if(!check_with_Class(glyph_array, *index - 1, backtrackClassDef, (uint16_t *)((uint8_t *)backtrackSequenceRule + sizeof(uint16_t) * 1), backtrackGlyphCount, -1)) {
          continue;
        }
        if(!check_with_Class(glyph_array, *index + inputGlyphCount, lookaheadClassDef, (uint16_t *)((uint8_t *)lookaheadSequenceRule + sizeof(uint16_t) * 1), lookaheadGlyphCount, +1)) {
          continue;
        }

        apply_SequenceRule(chain, inputGlyphCount, sequenceRule->seqLookupRecords, seqLookupCount, glyph_array, index);
        // Only use the first one that matches.
        return true;
      }
      break;
    }
    case ChainedSequenceContextFormat_3: {
      const ChainedSequenceContextFormat3_backtrack *backtrackCoverage = (ChainedSequenceContextFormat3_backtrack *)((uint8_t *)genericChainedSequence + sizeof(uint16_t));
      uint16_t backtrackGlyphCount = parse_16(backtrackCoverage->backtrackGlyphCount);
      const ChainedSequenceContextFormat3_input *inputCoverage = (ChainedSequenceContextFormat3_input *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * (backtrackGlyphCount + 1));
      uint16_t inputGlyphCount = parse_16(inputCoverage->inputGlyphCount);
      const ChainedSequenceContextFormat3_lookahead *lookaheadCoverage = (ChainedSequenceContextFormat3_lookahead *)((uint8_t *)inputCoverage + sizeof(uint16_t) * (inputGlyphCount + 1));
      uint16_t lookaheadGlyphCount = parse_16(lookaheadCoverage->lookaheadGlyphCount);
      const ChainedSequenceContextFormat3_seq *seqCoverage = (ChainedSequenceContextFormat3_seq *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * (lookaheadGlyphCount + 1));
      uint16_t seqLookupCount = parse_16(seqCoverage->seqLookupCount);

      if (*index + inputGlyphCount + lookaheadGlyphCount > glyph_array->len) {
        return false;
      }
      if (backtrackGlyphCount > *index) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index, (uint8_t *)genericChainedSequence, (uint16_t *)((uint8_t *)inputCoverage + sizeof(uint16_t) * 1), inputGlyphCount, +1)) {
        return false;
      }
      // backtrack is defined with inverse order, so glyph index - 2 will be backtrack coverage index 2
      if (!check_with_Coverage(glyph_array, *index - 1, (uint8_t *)genericChainedSequence, (uint16_t *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * 1), backtrackGlyphCount, -1)) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, *index + inputGlyphCount, (uint8_t *)genericChainedSequence, (uint16_t *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * 1), lookaheadGlyphCount, +1)) {
        return false;
      }

      if (inputGlyphCount == 0) return true;

      apply_SequenceRule(chain, inputGlyphCount, seqCoverage->seqLookupRecords, seqLookupCount, glyph_array, index);
      return true;
    }
    default:
      fprintf(stderr, "UNKNOWN ChainedSequenceContextFormat %d\n", parse_16(genericChainedSequence->format));
      break;
  }
  return false;
}


static bool apply_ReverseChainingContextSingleLookupType(const ReverseChainSingleSubstFormat1 *reverseChain, GlyphArray* glyph_array, size_t index) {
  CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)reverseChain + parse_16(reverseChain->coverageOffset));
  switch (parse_16(reverseChain->substFormat)) {
    case ReverseChainSingleSubstFormat_1: {
      uint32_t coverage_index;
      bool applicable = find_in_Coverage(coverageTable, glyph_array->array[index], &coverage_index);
      if (!applicable) return false;

      const ReverseChainSingleSubstFormat1_backtrack *backtrackCoverage = (ReverseChainSingleSubstFormat1_backtrack *)((uint8_t *)reverseChain + sizeof(uint16_t) * 2);
      uint16_t backtrackGlyphCount = parse_16(backtrackCoverage->backtrackGlyphCount);
      const ReverseChainSingleSubstFormat1_lookahead *lookaheadCoverage = (ReverseChainSingleSubstFormat1_lookahead *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * (backtrackGlyphCount + 1));
      uint16_t lookaheadGlyphCount = parse_16(lookaheadCoverage->lookaheadGlyphCount);
      const ReverseChainSingleSubstFormat1_sub *substitutionTable = (ReverseChainSingleSubstFormat1_sub *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * (lookaheadGlyphCount + 1));

      if (index + lookaheadGlyphCount >= glyph_array->len) {
        return false;
      }
      if (backtrackGlyphCount > index) {
        return false;
      }
      // backtrack is defined with inverse order, so glyph index - 2 will be backtrack coverage index 2
      if (!check_with_Coverage(glyph_array, index - 1, (uint8_t *)reverseChain, (uint16_t *)((uint8_t *)backtrackCoverage + sizeof(uint16_t) * 1), backtrackGlyphCount, -1)) {
        return false;
      }
      if (!check_with_Coverage(glyph_array, index + 1, (uint8_t *)reverseChain, (uint16_t *)((uint8_t *)lookaheadCoverage + sizeof(uint16_t) * 1), lookaheadGlyphCount, +1)) {
        return false;
      }

      if (coverage_index > parse_16(substitutionTable->glyphCount)) {
        fprintf(stderr, "Possible mistake in ReverseChainingContextSingleLookupType implementation\n");
        return false;
      }
      GlyphArray_set1(glyph_array, index, parse_16(substitutionTable->substituteGlyphIDs[coverage_index]));
      return true;
    }
    default:
      fprintf(stderr, "UNKNOWN ReverseChainSingleSubstFormat %d\n", parse_16(reverseChain->substFormat));
      break;
  }
  return false;
}

static bool apply_Substitution(const Chain *chain, GlyphArray* glyph_array, const GenericSubstTable *genericSubstTable, uint16_t lookupType, size_t *index) {
  switch (lookupType) {
    case SingleLookupType: {
      const SingleSubstFormatGeneric *singleSubstFormatGeneric = (SingleSubstFormatGeneric *)genericSubstTable;
      return apply_SingleSubstitution(singleSubstFormatGeneric, glyph_array, *index);
    }
    case MultipleLookupType: {
      const MultipleSubstFormat1 *multipleSubstFormat = (MultipleSubstFormat1 *)genericSubstTable;
      // Stop at the first one we apply
      return apply_MultipleSubstitution(multipleSubstFormat, glyph_array, index);
    }
    case AlternateLookupType: // TODO: just filter those out at chain creation time...
      // We don't really need to support it.
      // Most use-cases revolve around user selection from the list of alternates,
      // which we don't... really care about.
      // Maybe we could think about enabling this for some weird features like 'rand'.
      fprintf(stderr, "AlternateLookupType unsupported\n");
      return false;
    case LigatureLookupType: {
      const LigatureSubstitutionTable *ligatureSubstitutionTable = (LigatureSubstitutionTable *)genericSubstTable;
      // Stop at the first one we apply
      return apply_LigatureSubstitution(ligatureSubstitutionTable, glyph_array, *index);
    }
    case ContextLookupType: {
      const GenericSequenceContextFormat *genericSequence = (GenericSequenceContextFormat *)genericSubstTable;
      return apply_SequenceSubstitution(chain, genericSequence, glyph_array, index);
    }
    case ChainingLookupType: {
      const GenericChainedSequenceContextFormat *genericChainedSequence = (GenericChainedSequenceContextFormat *)genericSubstTable;
      return apply_ChainedSequenceSubstitution(chain, genericChainedSequence, glyph_array, index);
    }
    case ExtensionSubstitutionLookupType: {
      const ExtensionSubstitutionTable *extensionSubstitutionTable = (ExtensionSubstitutionTable *)genericSubstTable;
      const GenericSubstTable *_genericSubstTable = (GenericSubstTable *)((uint8_t *)extensionSubstitutionTable + parse_32(extensionSubstitutionTable->extensionOffset));
      return apply_Substitution(chain, glyph_array, _genericSubstTable, parse_16(extensionSubstitutionTable->extensionLookupType), index);
    }
    case ReverseChainingContextSingleLookupType: {
      const ReverseChainSingleSubstFormat1 *reverseChain = (ReverseChainSingleSubstFormat1 *)genericSubstTable;
      // Stop at the first one we apply
      return apply_ReverseChainingContextSingleLookupType(reverseChain, glyph_array, *index);
    }
    default:
      fprintf(stderr, "UNKNOWN LookupType\n");
      return false;
  }
}

// Returns the bloom digest that matches with the glyphs that "start" a Substitution table.
static Bloom get_Substitution_bloom(const GenericSubstTable *genericSubstTable, uint16_t lookupType) {
  switch (lookupType) {
    case SingleLookupType: {
      const SingleSubstFormatGeneric *singleSubstFormatGeneric = (SingleSubstFormatGeneric *)genericSubstTable;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)singleSubstFormatGeneric + parse_16(singleSubstFormatGeneric->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    case MultipleLookupType: {
      const MultipleSubstFormat1 *multipleSubstFormat = (MultipleSubstFormat1 *)genericSubstTable;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)multipleSubstFormat + parse_16(multipleSubstFormat->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    case AlternateLookupType:
      // No need to make it go through, as we don't support it
      return null_bloom;
    case LigatureLookupType: {
      const LigatureSubstitutionTable *ligatureSubstitutionTable = (LigatureSubstitutionTable *)genericSubstTable;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)ligatureSubstitutionTable + parse_16(ligatureSubstitutionTable->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    case ContextLookupType: {
      const GenericSequenceContextFormat *genericSequence = (GenericSequenceContextFormat *)genericSubstTable;
      return get_SequenceSubstitution_bloom(genericSequence);
    }
    case ChainingLookupType: {
      const GenericChainedSequenceContextFormat *genericChainedSequence = (GenericChainedSequenceContextFormat *)genericSubstTable;
      return get_ChainedSequenceSubstitution_bloom(genericChainedSequence);
    }
    case ExtensionSubstitutionLookupType: {
      const ExtensionSubstitutionTable *extensionSubstitutionTable = (ExtensionSubstitutionTable *)genericSubstTable;
      const GenericSubstTable *_genericSubstTable = (GenericSubstTable *)((uint8_t *)extensionSubstitutionTable + parse_32(extensionSubstitutionTable->extensionOffset));
      return get_Substitution_bloom(_genericSubstTable, parse_16(extensionSubstitutionTable->extensionLookupType));
    }
    case ReverseChainingContextSingleLookupType: {
      const ReverseChainSingleSubstFormat1 *reverseChain = (ReverseChainSingleSubstFormat1 *)genericSubstTable;
      const CoverageTable *coverageTable = (CoverageTable *)((uint8_t *)reverseChain + parse_16(reverseChain->coverageOffset));
      return get_Coverage_bloom(coverageTable);
    }
    default:
      fprintf(stderr, "UNKNOWN LookupType\n");
      return full_bloom;
  }
}

// Returns the bloom digest that matches with the glyphs that "start" any of the
// Substitution tables of the Lookup.
static Bloom get_cached_Lookup_bloom(const Chain *chain, const LookupTable *lookupTable, uint16_t lookupType) {
  Bloom lookup_bloom = null_bloom;
  if (!get_from_Bloom_hash(chain->bloom_hash, lookupTable, &lookup_bloom)) {
    uint16_t subTableCount = parse_16(lookupTable->subTableCount);
    for (uint16_t i = 0; i < subTableCount; i++) {
      const GenericSubstTable *genericSubstTable = (GenericSubstTable *)((uint8_t *)lookupTable + parse_16(lookupTable->subtableOffsets[i]));
      Bloom _bloom = null_bloom;
      if (!get_from_Bloom_hash(chain->bloom_hash, genericSubstTable, &_bloom)) {
        _bloom = get_Substitution_bloom(genericSubstTable, lookupType);
        set_to_Bloom_hash(chain->bloom_hash, genericSubstTable, _bloom);
      }
      lookup_bloom = add_bloom_to_bloom(lookup_bloom, _bloom);
      if (is_full_bloom(lookup_bloom)) break;
    }
    set_to_Bloom_hash(chain->bloom_hash, lookupTable, lookup_bloom);
  }
  return lookup_bloom;
}

// Returns the bloom digests for all the Substitution tables of the Lookup.
static Bloom *get_cached_Substitution_blooms_for_Lookup(const Chain *chain, const LookupTable *lookupTable, uint16_t lookupType) {
  Bloom* sub_blooms = NULL;
  if (!get_from_uintptr_t_hash(chain->ptr_hash, lookupTable, (uintptr_t*)&sub_blooms)){
    uint16_t subTableCount = parse_16(lookupTable->subTableCount);
    sub_blooms = malloc(subTableCount * sizeof(Bloom));
    if (sub_blooms == NULL) return NULL; // TODO: panic?

    for (uint16_t i = 0; i < subTableCount; i++) {
      const GenericSubstTable *genericSubstTable = (GenericSubstTable *)((uint8_t *)lookupTable + parse_16(lookupTable->subtableOffsets[i]));
      Bloom _bloom = null_bloom;
      if (!get_from_Bloom_hash(chain->bloom_hash, genericSubstTable, &_bloom)) {
        _bloom = get_Substitution_bloom(genericSubstTable, lookupType);
        set_to_Bloom_hash(chain->bloom_hash, genericSubstTable, _bloom);
      }
      sub_blooms[i] = _bloom;
    }
    set_to_uintptr_t_hash(chain->ptr_hash, lookupTable, (uintptr_t)sub_blooms);
  }
  return sub_blooms;
}

static void apply_Lookup_at_index(const Chain *chain, const LookupTable *lookupTable, const Bloom* blooms, GlyphArray* glyph_array, size_t *index) {
  uint16_t lookupType = parse_16(lookupTable->lookupType);
  uint16_t glyphID = glyph_array->array[*index];
  Bloom glyphID_bloom = get_glyphID_bloom(glyphID);

  // Obtain the bloom digests of the Substitution tables of the Lookup.
  // We get NULL here when we come from apply_SequenceRule.
  if (blooms == NULL) {
    blooms = get_cached_Substitution_blooms_for_Lookup(chain, lookupTable, lookupType);
  }

  // Stop at the first Substitution that's successfully applied.
  uint16_t subTableCount = parse_16(lookupTable->subTableCount);
  for (uint16_t i = 0; i < subTableCount; i++) {
    const GenericSubstTable *genericSubstTable = (GenericSubstTable *)((uint8_t *)lookupTable + parse_16(lookupTable->subtableOffsets[i]));
    if (blooms != NULL) {
      // If the glyph doesn't match the bloom digest for the Substitution, skip it.
      if (!glyphID_bloom_compare_bloom(glyphID_bloom, blooms[i])) {
        continue;
      }
    }
    if (apply_Substitution(chain, glyph_array, genericSubstTable, lookupType, index)) {
      break;
    }
  }
}

static void apply_Lookup(const Chain *chain, const LookupTable *lookupTable, GlyphArray* glyph_array) {
  size_t index = 0, reverse_index = glyph_array->len - 1, *index_ptr = &index;
  uint16_t lookupType = parse_16(lookupTable->lookupType);
  // ReverseChaining needs to be applied in reverse order.
  if (lookupType == ReverseChainingContextSingleLookupType)
    index_ptr = &reverse_index;

  // Get bloom for the whole lookup.
  Bloom lookup_bloom = get_cached_Lookup_bloom(chain, lookupTable, lookupType);

  // If no glyph in the input matches any of the Substitutions, skip the Lookup.
  Bloom ga_bloom = GlyphArray_get_bloom(glyph_array);
  if (!bloom_compare_bloom(ga_bloom, lookup_bloom)) {
    return;
  }

  // Extract list of blooms from the hashtable to let apply_Lookup_at_index skip
  // getting them for each glyph.
  Bloom* sub_blooms = get_cached_Substitution_blooms_for_Lookup(chain, lookupTable, lookupType);

  while (index < glyph_array->len) {
    // If the current glyph doesn't match any of the Substitutions, skip it.
    if ((glyphID_compare_bloom(glyph_array->array[*index_ptr], lookup_bloom))) {
      apply_Lookup_at_index(chain, lookupTable, sub_blooms, glyph_array, index_ptr);
    }
    index++;
    // ReverseChaining doesn't change the number of glyphs, so we can just do --.
    reverse_index--;
  }
}

void apply_chain(const Chain *chain, GlyphArray* glyph_array) {
  for (size_t i = 0; i < chain->lookupCount; i++) {
    apply_Lookup(chain, chain->lookupsArray[i], glyph_array);
  }
}

