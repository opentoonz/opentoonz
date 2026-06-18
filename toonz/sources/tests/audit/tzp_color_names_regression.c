#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/tzp/tcm.h"

static int fail(const char *message) {
  fprintf(stderr, "%s\n", message);
  return 1;
}

static void free_color_item(void *data) {
  CDB_TREE_ITEM *item = (CDB_TREE_ITEM *)data;

  if (!item) return;
  free(item->group);
  free(item->name);
  free(item->accelerator);
  if (item->effects) avl_close(item->effects);
  free(item);
}

int main(void) {
  TCM_INFO one_color = Tcm_24_default_info;
  TREE *empty_tree;
  TREE *malformed_tree;
  char digits[160];
  char malformed[256];

  one_color.n_colors  = 1;
  one_color.n_pencils = 0;

  empty_tree = cdb_decode_all(NULL, one_color);
  if (!empty_tree) return fail("expected null color metadata to decode");
  if (avl_nodes(empty_tree) != 1)
    return fail("expected null color metadata to produce one default entry");
  avl_release(empty_tree, free_color_item);
  avl_close(empty_tree);

  memset(digits, '9', sizeof(digits) - 1);
  digits[sizeof(digits) - 1] = '\0';
  snprintf(malformed, sizeof(malformed), "group\tname\teffect#%s#", digits);

  malformed_tree = cdb_decode_all(malformed, one_color);
  if (!malformed_tree) return fail("expected malformed metadata to return a tree");
  if (avl_nodes(malformed_tree) != 0)
    return fail("expected malformed effect metadata to be rejected");
  avl_close(malformed_tree);

  return 0;
}
