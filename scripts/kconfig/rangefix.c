#include "rangefix.h"

#define CONFIG_DEBUG 1

#if CONFIG_DEBUG
#define DEBUG(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

int rangefix_init(const char *kconfig_file) {
  conf_parse(kconfig_file);
  printf("test\n");
  return 1;
}

void rangefix_run(struct symbol *sym, tristate val) {
}
