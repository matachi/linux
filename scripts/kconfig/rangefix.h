#ifndef RANGEFIX_H
#define RANGEFIX_H

#define LKC_DIRECT_LINK
#include "lkc.h"

int rangefix_init(const char *, const char*);
int rangefix_run(const char *sym, tristate val);
GArray *rangefix_generate_diagnoses(void);
GArray *rangefix_get_constraints(void);
struct expr *rangefix_to_one_constraint(GArray *constraints);
struct expr *rangefix_get_modified_constraint(
	struct expr *constraints, GArray *diagnosis);

#endif
