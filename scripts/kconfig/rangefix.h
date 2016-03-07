#ifndef RANGEFIX_H
#define RANGEFIX_H

#define LKC_DIRECT_LINK
#include "lkc.h"

int rangefix_init(const char *, const char*);
int rangefix_run(const char *sym, tristate val);
GArray *rangefix_generate_diagnoses(void);

#endif
