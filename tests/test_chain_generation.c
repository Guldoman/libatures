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

static bool test_generate_chain(void) {
  LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, NULL, 0);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static bool test_generate_chain_good_script(void) {
  const unsigned char script[4] = {'l', 'a', 't', 'n'};
  LBT_Chain *c = LBT_generate_chain(cc, &script, NULL, NULL, 0);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static bool test_generate_chain_bad_script(void) {
  const unsigned char script[4] = {'A', 'A', 'A', 'A'};
  LBT_Chain *c = LBT_generate_chain(cc, &script, NULL, NULL, 0);
  if (c == NULL) return true;
  LBT_destroy_chain(c);
  return false;
}

static bool test_generate_chain_DFLT_script(void) {
  const unsigned char script[4] = {'D', 'F', 'L', 'T'};
  LBT_Chain *c = LBT_generate_chain(cc, &script, NULL, NULL, 0);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static bool test_generate_chain_DFLT_lang(void) {
  const unsigned char lang[4] = {'D', 'F', 'L', 'T'};
  LBT_Chain *c = LBT_generate_chain(cc, NULL, &lang, NULL, 0);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static bool test_generate_chain_bad_lang(void) {
  const unsigned char lang[4] = {'A', 'A', 'A', 'A'};
  LBT_Chain *c = LBT_generate_chain(cc, NULL, &lang, NULL, 0);
  if (c == NULL) return true;
  LBT_destroy_chain(c);
  return false;
}

static bool test_generate_chain_ccmp_feature(void) {
  const unsigned char feature[4] = {'c', 'c', 'm', 'p'};
  LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, &feature, 1);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static bool test_generate_chain_bad_feature(void) {
  const unsigned char feature[4] = {'A', 'A', 'A', 'A'};
  LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, &feature, 1);
  if (c == NULL) return false;
  LBT_destroy_chain(c);
  return true;
}

static tap_test tests[] = {
  { "Chain with default arguments", test_generate_chain, TAP_RUN },
  { "Chain with `latn` script", test_generate_chain_good_script, TAP_RUN },
  { "Chain with (bad) `AAAA` script", test_generate_chain_bad_script, TAP_RUN },
  { "Chain with `DFLT` script", test_generate_chain_DFLT_script, TAP_RUN },
  { "Chain with `DFLT` lang", test_generate_chain_DFLT_lang, TAP_RUN },
  { "Chain with (bad) `AAAA` lang", test_generate_chain_bad_lang, TAP_RUN },
  { "Chain with `ccmp` feature", test_generate_chain_ccmp_feature, TAP_RUN },
  { "Chain with (bad, acceptable) `AAAA` feature", test_generate_chain_bad_feature, TAP_RUN },
};

int main() {
  init_freetype(&lib);
  load_font(lib, "tests/JetBrainsMono-Regular.ttf", &face);

  setup(face, &cc);
  tap_run_tests(tests);
  teardown(&cc);

  destroy_font(face);
  destroy_freetype(lib);
  return 0;
}
