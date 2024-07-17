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

make_test(no_substitutions,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "hello world",
  EXPECTED(252, 225, 275, 275, 290, 958, 362, 290, 320, 275, 221)
)
make_test(simple_substitution1,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "==",
  EXPECTED(1742, 1571)
)
make_test(simple_substitution2,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "->",
  EXPECTED(1742,  881)
)
make_test(simple_substitution3,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "<-",
  EXPECTED(1742, 1588)
)
make_test(multiple_substitutions1,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "-><-",
  EXPECTED(1742, 881, 1742, 1588)
)
make_test(multiple_substitutions2,
  NO_SCRIPT(), NO_LANG(),
  FEATS(LBT_make_tag("calt")),
  "-><-><==><=><-><--<<-<>",
  EXPECTED(1742, 881, 1742, 1742, 1591, 1742, 1742, 1742, 1610, 1742, 1742, 1611, 1742, 1742, 1591, 1742, 1742, 1589, 1742, 1742, 1615, 1742, 1613)
)

static tap_test tests[] = {
  { "No substitutions",        test_no_substitutions,        TAP_RUN },
  { "Simple substitution1",    test_simple_substitution1,    TAP_RUN },
  { "Simple substitution2",    test_simple_substitution2,    TAP_RUN },
  { "Simple substitution3",    test_simple_substitution3,    TAP_RUN },
  { "Multiple substitutions1", test_multiple_substitutions1, TAP_RUN },
  { "Multiple substitutions2", test_multiple_substitutions2, TAP_RUN },
};

int main(void) {
  init_freetype(&lib);
  load_font(lib, "tests/JetBrainsMono-Regular.ttf", &face);

  setup(face, &cc);
  tap_run_tests(tests);
  teardown(&cc);
  return 0;
}
