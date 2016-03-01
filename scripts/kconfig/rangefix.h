#ifndef RANGEFIX_H
#define RANGEFIX_H

#define LKC_DIRECT_LINK
#include "lkc.h"

int rangefix_init(const char *);
void rangefix_run(struct symbol *sym, tristate val);

#endif
