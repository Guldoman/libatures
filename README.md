# `libatures`

This is a simple library only intended for applying ligatures to glyph ID runs.
If more advanced shaping features are needed,
this might not be the right library to use.

## API

Look at [libatures.h](src/libatures.h), as it documents the whole API.

### Example

```c
FT_Library lib;
FT_Face face;
FT_Init_FreeType(&lib);
FT_New_Face(lib, "JetBrainsMono-Regular.ttf", 0, &face);

LBT_ChainCreator *cc = LBT_new(face);

LBT_tag features[] = { LBT_make_tag("calt"), LBT_make_tag("ccmp") };
LBT_Chain *c = LBT_generate_chain(cc, NULL, NULL, features, 2);

const LBT_Glyph str[] = {855, 1051}; // "->" in Glyph IDs
size_t out_len;
LBT_Glyph* ligated = LBT_apply_chain(c, str, 2, &out_len);
for (size_t i = 0; i < out_len; i++)
  printf("%d ", ligated[i]);
// Outputs "1742 881" which are the Glyph IDs that result in the → glyph
```
