#pragma once
#include <stdint.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

typedef uint64_t bloom_part;
typedef struct Bloom {
  bloom_part a, b, c;
} Bloom;

// Bloom digest implementation inspired from Harfbuzz's

#define bloom_shift_a 9
#define bloom_shift_b 0
#define bloom_shift_c 4
#define mask_bits (ssize_t)(sizeof(bloom_part) * 8)
#define get_mask(x, shift) ((bloom_part) 1) << (((x) >> (shift)) & (mask_bits - 1))

#define null_bloom ((Bloom){.a = 0, .b = 0, .c = 0})
#define full_bloom ((Bloom){.a = (bloom_part)-1,                               \
                            .b = (bloom_part)-1,                               \
                            .c = (bloom_part)-1})

#define is_full_bloom(x) ((x).a == full_bloom.a &&                             \
                          (x).b == full_bloom.b &&                             \
                          (x).c == full_bloom.c)

#define get_glyphID_bloom(x) ((Bloom){.a = get_mask((x), (bloom_shift_a)),     \
                                      .b = get_mask((x), (bloom_shift_b)),     \
                                      .c = get_mask((x), (bloom_shift_c))})

static inline Bloom get_glyphID_range_bloom(uint16_t glyphID_a, uint16_t glyphID_b) {
  Bloom new_bloom = full_bloom;
  if ((glyphID_b >> bloom_shift_a) - (glyphID_a >> bloom_shift_a) < mask_bits - 1) {
    bloom_part part_a = get_mask(glyphID_a, bloom_shift_a);
    bloom_part part_b = get_mask(glyphID_b, bloom_shift_a);
    new_bloom.a = part_b + (part_b - part_a) - (part_b < part_a);
  }
  if ((glyphID_b >> bloom_shift_b) - (glyphID_a >> bloom_shift_b) < mask_bits - 1) {
    bloom_part part_a = get_mask(glyphID_a, bloom_shift_b);
    bloom_part part_b = get_mask(glyphID_b, bloom_shift_b);
    new_bloom.b = part_b + (part_b - part_a) - (part_b < part_a);
  }
  if ((glyphID_b >> bloom_shift_c) - (glyphID_a >> bloom_shift_c) < mask_bits - 1) {
    bloom_part part_a = get_mask(glyphID_a, bloom_shift_c);
    bloom_part part_b = get_mask(glyphID_b, bloom_shift_c);
    new_bloom.c = part_b + (part_b - part_a) - (part_b < part_a);
  }
  return new_bloom;
}

#define add_glyphID_to_bloom(bloom, x) ((Bloom){.a = (bloom).a | get_glyphID_bloom(x).a,\
                                                .b = (bloom).b | get_glyphID_bloom(x).b,\
                                                .c = (bloom).c | get_glyphID_bloom(x).c})

#define add_bloom_to_bloom(bloom_a, bloom_b) ((Bloom){.a = (bloom_a).a | (bloom_b).a,\
                                                      .b = (bloom_a).b | (bloom_b).b,\
                                                      .c = (bloom_a).c | (bloom_b).c})

#define bloom_compare_bloom(bloom_a, bloom_b) ((bloom_a).a & (bloom_b).a &&    \
                                               (bloom_a).b & (bloom_b).b &&    \
                                               (bloom_a).c & (bloom_b).c)

#define glyphID_bloom_compare_bloom(glyphID_bloom, bloom) (((glyphID_bloom).a & (bloom).a) == (glyphID_bloom).a &&\
                                                           ((glyphID_bloom).b & (bloom).b) == (glyphID_bloom).b &&\
                                                           ((glyphID_bloom).c & (bloom).c) == (glyphID_bloom).c)

#define glyphID_compare_bloom(glyphID, bloom) (glyphID_bloom_compare_bloom(get_glyphID_bloom(glyphID), (bloom)))

