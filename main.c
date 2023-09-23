#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "clap.h"
#define NDEBUG
#define VEC_IMPL
#include "vec.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// TODO: include stdint lol
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

typedef struct {
  f32 r, g, b, a;
} color;

typedef struct {
  u8 r, g, b, a;
} u8color;

void color_print(color c) {
  printf("{%.3f, %.3f, %.3f, %.3f}\n", c.r, c.g, c.b, c.a);
}

f32 color_sq_dist(color a, color b) {
  return (a.r - b.r) * (a.r - b.r) +
    (a.g - b.g) * (a.g - b.g) +
    (a.b - b.b) * (a.b - b.b) +
    (a.a - b.a) * (a.a - b.a);
}

color color_add(color a, color b) {
  return (color) {
    .r = a.r + b.r,
    .g = a.g + b.g,
    .b = a.b + b.b,
    .a = a.a + b.a
  };
}

color color_divs(color a, f64 s) {
  return (color) {
    .r = a.r / s,
    .g = a.g / s,
    .b = a.b / s,
    .a = a.a / s
  };
}

u8color u8color_from(color a) {
  return (u8color) {.r = a.r * 255, .g = a.g * 255, .b = a.b * 255, .a = a.a * 255};
}

void kmeans_iter(arr_t img, arr_t k_vals);

int main(int argc, char **argv) {
  srand(time(0));
  clap_parser parser = clap_parser_init(argc, argv, (clap_parser_opts) {
      .desc = "Simple program to reduce an image into <k> colors using the \
k-means algorithm"
    });
  clap_arg_add(&parser, (clap_arg) {
      .description = "In file path",
      .options = CLAP_ARG_UNNAMED
    });
  clap_arg_add(&parser, (clap_arg) {
      .name = "output",
      .alias = 'o',
      .description = "Out file path",
      .options = CLAP_ARG_OPTIONAL
    });
  clap_arg_add(&parser, (clap_arg) {
      .name = "k-number",
      .alias = 'k',
      .description = "Number of colors in the final image"
    });
  if (!clap_parse(&parser)) {
    clap_print_err(parser);
    clap_destroy(&parser);
    return 1;
  }
  const char *in_file = clap_get_unnamed(parser, 0); // get the first unnamed arg
  if (!in_file) {
    fprintf(stderr, "No valid input file specified\n");
    return 1;
  }
  const char *out_file = clap_get(parser, "output");
  if (!out_file) {
    out_file = "out.png";
  }
  const char *k_str = clap_get(parser, "k-number");
  const int k = strtol(k_str, NULL, 10);
  if (k <= 0) {
    fprintf(stderr, "k must be a positive integer\n");
    return 1;
  }

  printf("Working on %s ", in_file);
  printf("to produce an image with %d colors in %s\n", k, out_file);

  int w, h, n;
  stbi_ldr_to_hdr_gamma(1.f);
  f32 *c_data = stbi_loadf(in_file, &w, &h, &n, 4);
  arr_t img_data = arr_from(color, c_data, w*h);

  // TIMING
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  arr_t k_vals = arr_new(color, k);
  arr_for_each(color, k_vals, it) {
    *it = (color) {
      .r = (float)rand() / RAND_MAX,
      .g = (float)rand() / RAND_MAX,
      .b = (float)rand() / RAND_MAX,
      .a = 1.f
    };
  }
  // TODO: apply the algorithm
  for (size_t i = 0; i < 10; ++i) {
    kmeans_iter(img_data, k_vals);
  }

  arr_t out_data = arr_new(u8color, w * h);
  // get the closest
  // then construct the final image
  for (size_t i = 0; i < img_data.len; ++i) {
    f32 min_sq_dist = 4.1;
    size_t idx = 0;
    for (size_t j = 0; j < k_vals.len; j++) {
      f32 sq_dist = color_sq_dist(arr_at(color, img_data, i), arr_at(color, k_vals, j));
      if (sq_dist < min_sq_dist) {
	min_sq_dist = sq_dist;
	idx = j;
      }
    }
    *arr_ref(u8color, out_data, i) = u8color_from(arr_at(color, k_vals, idx));
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 +
    (end.tv_nsec - start.tv_nsec) / 1000;
  printf("Done! Processed in %lu us\n", delta_us);
  
  int ok = stbi_write_png(out_file, w, h, 4, out_data.data, 4 * w);
  if (ok == 0) {
    fprintf(stderr, "Error writing to png at %s\n", out_file);
  }
  
  printf("Done! Wrote output to %s\n", out_file);
  // TODO: write to file
  arr_free(out_data);
  arr_free(k_vals);
  arr_free(img_data);
  clap_destroy(&parser);
}

// k_vals is an inout parameter
void kmeans_iter(arr_t img, arr_t k_vals) {
  arr_t k_val_sums = arr_new(color, k_vals.len);
  memset(k_val_sums.data, 0, k_val_sums.len * sizeof(color));
  arr_t k_val_counts = arr_new(size_t, k_vals.len);
  memset(k_val_counts.data, 0, k_val_sums.len * sizeof(size_t));

  arr_for_each(color, img, it) {
    f32 min_sq_dist = 4.1;
    size_t idx = 0;
    for (size_t j = 0; j < k_vals.len; j++) {
      f32 sq_dist = color_sq_dist(*it, arr_at(color, k_vals, j));
      if (sq_dist < min_sq_dist) {
	min_sq_dist = sq_dist;
	idx = j;
      }
    }
    *arr_ref(color, k_val_sums, idx) =
      color_add(arr_at(color, k_val_sums, idx), *it);
    *arr_ref(size_t, k_val_counts, idx) += 1;
  }

  // arr_for_each(size_t, k_val_counts, it) {
  //   printf("%zu ", *it);
  // }
  // printf("\n");
  
  for (size_t i = 0; i < k_val_sums.len; ++i) {
    if (arr_at(size_t, k_val_counts, i) > 0) {
      *arr_ref(color, k_val_sums, i) =
	color_divs(arr_at(color, k_val_sums, i),
		      arr_at(size_t, k_val_counts, i));
    } else {
      // if count was zero, re-randomize the color
      *arr_ref(color, k_val_sums, i) = (color) {
	.r = (float)rand() / RAND_MAX,
	.g = (float)rand() / RAND_MAX,
	.b = (float)rand() / RAND_MAX,
	.a = 1.f
      };
    }     
  }
  
  for (size_t i = 0; i < k_val_sums.len; ++i) {
    *arr_ref(color, k_vals, i) = arr_at(color, k_val_sums, i);
  }

  arr_free(k_val_counts);
  arr_free(k_val_sums);
}
