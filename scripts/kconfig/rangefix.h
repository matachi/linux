#ifndef RANGEFIX_H
#define RANGEFIX_H

#include <glib.h>
#include "rangefixexpr.h"
#define LKC_DIRECT_LINK
#include "lkc.h"

#ifdef __cplusplus
  extern "C" {
#endif

gint64 time_find_diagnoses;
gint64 time_simplify_diagnoses;
unsigned int iterations;

int rangefix_init(const char *, const char *, bool load);
GArray *rangefix_run(struct symbol *sym, tristate);
GArray *rangefix_run_str(const char *sym, const char *val);
GArray *rangefix_generate_diagnoses(void);
GArray *rangefix_get_constraints(void);
struct r_expr *rangefix_to_one_constraint(GArray *constraints);
struct r_expr *rangefix_get_modified_constraint(
	struct r_expr *constraints, GArray *diagnosis);
GArray *rangefix_get_fixes(void);

/* TODO: Only temporary for debug reasons. */
GArray *full_diagnoses;

#ifdef __cplusplus
  }
#endif

#endif
