#include "bool.h"
#include "picosat.h"
#include "rangefix.h"
#include "satconf.h"

#define CONFIG_DEBUG 1

#if CONFIG_DEBUG
#define DEBUG(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

int rangefix_init(const char *kconfig_file)
{
	conf_parse(kconfig_file);
	conf_read(NULL);
	return 1;
}

void rangefix_run(struct symbol *sym, tristate val)
{
}
