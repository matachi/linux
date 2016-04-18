#ifndef RANGEFIXEXPR_H
#define RANGEFIXEXPR_H

#include <glib.h>
#include "expr.h"
#define LKC_DIRECT_LINK
#include "lkc.h"

/* A variable contains a pointer to a symbol, its state and whether it's on or
 * off. */
struct variable {
	struct symbol *sym;
	tristate tri;
	bool on;
};

/* The types we allow an expression to be built up by. */
enum r_expr_type {
	R_CONSTANT, R_VARIABLE, R_NOT, R_OR, R_AND
};

union r_expr_data {
	struct r_expr *expr;
	struct variable *var;
};

/* This is a pure boolean expression. A symbol whose type is tristate is split
 * into 3 variables. */
struct r_expr {
	enum r_expr_type type;
	union r_expr_data left, right;
};

struct variable *new_r_var(struct symbol *sym, tristate tri, bool on);
struct r_expr *new_r_expr_var(struct symbol *sym, tristate tri, bool on);
struct r_expr *new_r_expr_const(bool on);
struct r_expr *new_r_expr_not(struct r_expr *e);
struct r_expr *new_r_expr_and(struct r_expr *l, struct r_expr *r);
struct r_expr *new_r_expr_or(struct r_expr *l, struct r_expr *r);

/* Build a constraint for the prompt. The parameter 'tri' dictates the symbol's
 * value. */
struct r_expr *prompt_to_constraint(struct property *, tristate tri);

/* Build a constraint for the select. The parameter 'tri' dictates the symbol's
 * value. */
struct r_expr *select_to_constraint(struct property *, tristate tri);

void simplify_r_expr(struct r_expr *);
bool r_expr_contains_symbol(struct r_expr *, struct symbol *);
struct r_expr *r_expr_copy(struct r_expr *);
void r_expr_free(struct r_expr *);
gchar *r_expr_to_str(struct r_expr *);

#endif
