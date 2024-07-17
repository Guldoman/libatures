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

static bool test_no_features(void) {
  LBT_Glyph expected[] = { 1049, 1049 };
  return test_sub(cc, face, NULL, NULL, NULL, 0, "==", expected, 2);
}
make_test(single_feature,
  NO_SCRIPT(), NO_LANG(),
  FEATS("calt"),
  "==",
  EXPECTED(1742, 1571)
)
make_test(multiple_features,
  NO_SCRIPT(), NO_LANG(),
  FEATS("calt", "frac"),
  "==1/2",
  EXPECTED(1742, 1571, 761, 800, 752)
)
make_test(contrasting_features1,
  NO_SCRIPT(), NO_LANG(),
  FEATS("ss01", "ss02"),
  "f",
  EXPECTED(418)
)
make_test(contrasting_features2,
  NO_SCRIPT(), NO_LANG(),
  FEATS("ss02", "cv09"),
  "f",
  EXPECTED(451)
)
make_test(contrasting_features3,
  NO_SCRIPT(), NO_LANG(),
  FEATS("cv09", "cv17"),
  "f",
  EXPECTED(418)
)
make_test(contrasting_features4,
  NO_SCRIPT(), NO_LANG(),
  FEATS("ss01", "ss20"),
  "f",
  EXPECTED(452)
)
make_test(contrasting_features5,
  NO_SCRIPT(), NO_LANG(),
  FEATS("ss02", "ss20"),
  "f",
  EXPECTED(451)
)
make_test(contrasting_features6,
  NO_SCRIPT(), NO_LANG(),
  FEATS("cv09", "ss20"),
  "f",
  EXPECTED(453)
)
make_test(contrasting_features7,
  NO_SCRIPT(), NO_LANG(),
  FEATS("cv17", "ss20"),
  "f",
  EXPECTED(453)
)
make_test(unrelated_features,
  NO_SCRIPT(), NO_LANG(),
  FEATS("locl"),
  "==1/2",
  EXPECTED(1049, 1049, 725, 829, 726)
)

static tap_test tests[] = {
  { "No features",           test_no_features,           TAP_RUN },
  { "Single feature",        test_single_feature,        TAP_RUN },
  { "Multiple features",     test_multiple_features,     TAP_RUN },
  { "Contrasting features1", test_contrasting_features1, TAP_RUN },
  { "Contrasting features2", test_contrasting_features2, TAP_RUN },
  { "Contrasting features3", test_contrasting_features3, TAP_RUN },
  { "Contrasting features4", test_contrasting_features4, TAP_RUN },
  { "Contrasting features5", test_contrasting_features5, TAP_RUN },
  { "Contrasting features6", test_contrasting_features6, TAP_RUN },
  { "Contrasting features7", test_contrasting_features7, TAP_RUN },
  { "Unrelated features",    test_unrelated_features,    TAP_RUN },
};

int main(void) {
  init_freetype(&lib);
  load_font(lib, "tests/JetBrainsMono-Regular.ttf", &face);

  setup(face, &cc);
  tap_run_tests(tests);
  teardown(&cc);
  return 0;
}
