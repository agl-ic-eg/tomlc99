#include "../toml.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(TOML_MAX_DEPTH)
#define TOML_MAX_DEPTH (32)
#endif  // !defined(TOML_MAX_DEPTH)

enum shape { DOTTED, ARRAY, INLINE_TABLE, MIXED };
static size_t live_allocations;
static int cases;

static void *tracked_malloc(size_t size) {
  void *p = malloc(size);
  if (p)
    live_allocations++;
  return p;
}

static void tracked_free(void *p) {
  if (p) {
    assert(live_allocations > 0);
    live_allocations--;
  }
  free(p);
}

static char *append(char *p, const char *s) {
  size_t n = strlen(s);
  memcpy(p, s, n);
  return p + n;
}

static char *document(const char *prefix, enum shape shape, int depth) {
  char *buf = malloc(strlen(prefix) + (size_t)depth * 8 + 32);
  assert(buf);
  char *p = append(buf, prefix);
  if (shape == DOTTED) {
    for (int i = 0; i < depth; i++)
      p = append(p, "a.");
    p = append(p, "v = 1\n");
  } else {
    p = append(p, "v = ");
    for (int i = 0; i < depth; i++) {
      int table = shape == INLINE_TABLE || (shape == MIXED && i % 2);
      p = append(p, table ? "{ a = " : "[");
    }
    p = append(p, "1");
    for (int i = depth - 1; i >= 0; i--) {
      int table = shape == INLINE_TABLE || (shape == MIXED && i % 2);
      p = append(p, table ? "}" : "]");
    }
    p = append(p, "\n");
  }
  *p = 0;
  return buf;
}

/* expected: 1 = success, 0 = depth error, -1 = another syntax error. */
static void check(char *input, int expected, int from_file) {
  char errbuf[200];
  toml_table_t *tab;
  assert(live_allocations == 0);
  if (from_file) {
    FILE *fp = tmpfile();
    assert(fp);
    size_t len = strlen(input);
    assert(fwrite(input, 1, len, fp) == len);
    rewind(fp);
    tab = toml_parse_file(fp, errbuf, sizeof(errbuf));
    assert(fclose(fp) == 0);
  } else {
    tab = toml_parse(input, errbuf, sizeof(errbuf));
  }
  if (!!tab != (expected == 1)) {
    fprintf(stderr, "case %d: expected %d, got %s\n", cases, expected,
            tab ? "success" : errbuf);
    abort();
  }
  if (expected <= 0) {
    assert(errbuf[0]);
    assert((strstr(errbuf, "maximum nesting depth exceeded") != NULL) ==
           (expected == 0));
  }
  toml_free(tab);
  assert(live_allocations == 0);
  cases++;
}

static void check_depth(const char *prefix, enum shape shape, int depth,
                        int expected) {
  char *input = document(prefix, shape, depth);
  check(input, expected, 0);
  free(input);
}

int main(void) {
  toml_set_memutil(tracked_malloc, tracked_free);
  for (int shape = DOTTED; shape <= MIXED; shape++) {
    check_depth("", (enum shape)shape, 1, 1);
    check_depth("", (enum shape)shape, TOML_MAX_DEPTH, 1);
    check_depth("", (enum shape)shape, TOML_MAX_DEPTH + 1, 0);
    check_depth("", (enum shape)shape, 100000, 0);
    /* Reuse existing dotted-key tables, with a different terminal key. */
    check_depth("a.w = 2\n", (enum shape)shape, TOML_MAX_DEPTH, 1);
    /* Table headers create implicit parents outside the recursive parser. */
    check_depth("[a.b.c]\n", (enum shape)shape, TOML_MAX_DEPTH - 3, 1);
    check_depth("[a.b.c]\n", (enum shape)shape, TOML_MAX_DEPTH - 2, 0);
    /* An array-of-tables adds both an array and its element table. */
    check_depth("[[a.b.c]]\n", (enum shape)shape, TOML_MAX_DEPTH - 4, 1);
    check_depth("[[a.b.c]]\n", (enum shape)shape, TOML_MAX_DEPTH - 3, 0);
    /* Turning an implicit table into an explicit one preserves its depth. */
    check_depth("[a.b.c]\nx = 1\n[a]\n", (enum shape)shape, TOML_MAX_DEPTH - 1, 1);
    check_depth("[a.b.c]\nx = 1\n[a]\n", (enum shape)shape, TOML_MAX_DEPTH, 0);
  }

  char *input = document("", ARRAY, TOML_MAX_DEPTH + 1);
  check(input, 0, 1);
  free(input);
  input = document("", ARRAY, TOML_MAX_DEPTH);
  check(input, 1, 1);
  /* Fail after building the entire allowed-depth structure. */
  input[strlen(input) - 2] = 0;
  check(input, -1, 0);
  free(input);

  /* Cleanup after previous sibling values/containers have been created. */
  check_depth("other = [1, { k = 2 }]\n", MIXED, TOML_MAX_DEPTH + 1, 0);
  /* Brackets and dots in comments/quoted keys/strings are not nesting. */
  char ordinary[] = "# [[[[....\n\"a.b\" = \"[[[[....\"\n";
  check(ordinary, 1, 0);

  toml_set_memutil(malloc, free);
  printf("depth regression tests: %d cases passed\n", cases);
  return 0;
}
