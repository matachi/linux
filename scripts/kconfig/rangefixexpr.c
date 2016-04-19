#include <stdlib.h>
#include "expr.h"
#include "rangefixexpr.h"

struct variable *new_r_var(struct symbol *sym, tristate tri, bool on)
{
	struct variable *var = malloc(sizeof(struct variable));
	var->sym = sym;
	var->tri = tri;
	var->on = on;
	return var;
}

struct r_expr *new_r_expr_var(struct symbol *sym, tristate tri, bool on)
{
	struct r_expr *r = malloc(sizeof(struct r_expr));
	r->type = R_VARIABLE;
	r->left.var = new_r_var(sym, tri, on);
	return r;
}

struct r_expr *new_r_expr_const(bool on)
{
	struct r_expr *r = malloc(sizeof(struct r_expr));
	r->type = R_CONSTANT;
	r->left.var = new_r_var(NULL, 0, on);
	return r;
}

struct r_expr *new_r_expr_not(struct r_expr *e)
{
	struct r_expr *not = malloc(sizeof(struct r_expr));
	not->type = R_NOT;
	not->left.expr = e;
	return not;
}

struct r_expr *new_r_expr_and(struct r_expr *l, struct r_expr *r)
{
	struct r_expr *and = malloc(sizeof(struct r_expr));
	and->type = R_AND;
	and->left.expr = l;
	and->right.expr = r;
	return and;
}

struct r_expr *new_r_expr_or(struct r_expr *l, struct r_expr *r)
{
	struct r_expr *or = malloc(sizeof(struct r_expr));
	or->type = R_OR;
	or->left.expr = l;
	or->right.expr = r;
	return or;
}

static struct r_expr *expr_to_r_expr(
	struct expr *expr, tristate lower, tristate upper)
{
	struct r_expr *r;

	switch (expr->type) {
	case E_SYMBOL:
		switch (upper - lower) {
		case 0:
			r = new_r_expr_var(expr->left.sym, lower, true);
			break;
		case 1:
			r = new_r_expr_var(
				expr->left.sym,
				upper == yes ? no : yes,
				false
			);
			break;
		case 2:
			r = new_r_expr_or(
				new_r_expr_var(expr->left.sym, no, true),
				new_r_expr_or(
					new_r_expr_var(expr->left.sym, mod, true),
					new_r_expr_var(expr->left.sym, yes, true)
				)
			);
			break;
		}
		break;
	case E_NOT:
		;
		struct expr *child = expr->left.expr;
		if (child->type == E_SYMBOL) {
			r = expr_to_r_expr(child, 2 - upper, 2 - lower);
		} else {
			r = new_r_expr_not(
				expr_to_r_expr(child, lower, upper)
			);
		}
		break;
	case E_OR:
		r = new_r_expr_or(
			expr_to_r_expr(expr->left.expr, lower, upper),
			expr_to_r_expr(expr->right.expr, lower, upper)
		);
		break;
	case E_AND:
		r = new_r_expr_and(
			expr_to_r_expr(expr->left.expr, lower, upper),
			expr_to_r_expr(expr->right.expr, lower, upper)
		);
		break;
	case E_EQUAL:
		r = new_r_expr_var(
			expr->left.sym,
			expr->right.sym->curr.tri,
			true
		);
		break;
	case E_UNEQUAL:
		r = new_r_expr_var(
			expr->left.sym,
			expr->right.sym->curr.tri,
			false
		);
		break;
	default:
		fprintf(stderr, "Unhandled expr type: %i\n", expr->type);
	}
	return r;

}

static struct r_expr *expr_to_r_expr_same(struct expr *expr, tristate tri)
{
	return expr_to_r_expr(expr, tri, tri);
}

static struct r_expr *prompt_to_constraint_y(struct property *prop)
{
	return new_r_expr_or(
		new_r_expr_var(prop->sym, yes, false),
		expr_to_r_expr_same(prop->visible.expr, yes)
	);
}

static struct r_expr *prompt_to_constraint_m(struct property *prop)
{
	return new_r_expr_or(
		new_r_expr_var(prop->sym, mod, false),
		expr_to_r_expr(prop->visible.expr, mod, yes)
	);
}

struct r_expr *prompt_to_constraint(struct property *prop, tristate tri)
{
	if (!prop->visible.expr)
		return NULL;
	switch (tri) {
	case mod:
		return prompt_to_constraint_m(prop);
	case yes:
		return prompt_to_constraint_y(prop);
	}
}

static struct r_expr *select_to_constraint_y(struct property *prop)
{
	if (prop->visible.expr)
		return new_r_expr_or(
			new_r_expr_not(
				new_r_expr_and(
					new_r_expr_var(prop->sym, yes, true),
					expr_to_r_expr_same(
						prop->visible.expr, yes)
				)
			),
			expr_to_r_expr_same(prop->expr, yes)
		);
	else
		return new_r_expr_or(
			new_r_expr_var(prop->sym, yes, false),
			expr_to_r_expr_same(prop->expr, yes)
		);
}

static struct r_expr *select_to_constraint_m(struct property *prop)
{
	if (prop->visible.expr)
		return new_r_expr_or(
			new_r_expr_not(
				new_r_expr_and(
					new_r_expr_var(prop->sym, no, false),
					expr_to_r_expr(
						prop->visible.expr, mod, yes)
				)
			),
			expr_to_r_expr(prop->expr, mod, yes)
		);
	else
		return new_r_expr_or(
			new_r_expr_var(prop->sym, no, true),
			expr_to_r_expr(prop->expr, mod, yes)
		);
}

struct r_expr *select_to_constraint(struct property *prop, tristate tri)
{
	switch (tri) {
	case mod:
		return select_to_constraint_m(prop);
	case yes:
		return select_to_constraint_y(prop);
	}
}

static inline int r_expr_is_neg(struct r_expr *e)
{
	return e->type == R_CONSTANT && !e->left.var->on;
}

static inline int r_expr_is_pos(struct r_expr *e)
{
	return e->type == R_CONSTANT && e->left.var->on;
}

void simplify_r_expr(struct r_expr *e)
{
	struct r_expr *l, *r;
	struct variable *var;

	if (e == NULL ||
	    (e->type != R_NOT && e->type != R_AND && e->type != R_OR))
		return;

	l = e->left.expr;
	simplify_r_expr(l);

	switch (e->type) {
	case R_NOT:
		switch (l->type) {
		case R_CONSTANT:
			e->type = R_CONSTANT;
			var = new_r_var(NULL, 0, !l->left.var->on);
			e->left.var = var;
			r_expr_free(l);
			break;
		case R_VARIABLE:
			e->type = R_VARIABLE;
			var = new_r_var(
				l->left.var->sym,
				l->left.var->tri,
				!l->left.var->on);
			e->left.var = var;
			r_expr_free(l);
			break;
		}
		return;
	}

	r = e->right.expr;
	simplify_r_expr(r);

	switch (e->type) {
	case R_AND:
		if (r_expr_is_pos(l) && r_expr_is_pos(r)) {
			e->type = R_CONSTANT;
			e->left.var = new_r_var(NULL, 0, true);
			r_expr_free(l);
			r_expr_free(r);
		} else if (r_expr_is_neg(l) || r_expr_is_neg(r)) {
			e->type = R_CONSTANT;
			e->left.var = new_r_var(NULL, 0, false);
			r_expr_free(l);
			r_expr_free(r);
		} else if (r_expr_is_pos(l)) {
			e->type = r->type;
			e->left = r->left;
			e->right = r->right;
			r_expr_free(l);
		} else if (r_expr_is_pos(r)) {
			e->type = l->type;
			e->left = l->left;
			e->right = l->right;
			r_expr_free(r);
		}
		return;
	case R_OR:
		if (r_expr_is_pos(l) || r_expr_is_pos(r)) {
			e->type = R_CONSTANT;
			e->left.var = new_r_var(NULL, 0, true);
			r_expr_free(l);
			r_expr_free(r);
		} else if (r_expr_is_neg(l) && r_expr_is_neg(r)) {
			e->type = R_CONSTANT;
			e->left.var = new_r_var(NULL, 0, false);
			r_expr_free(l);
			r_expr_free(r);
		} else if (r_expr_is_neg(l)) {
			e->type = r->type;
			e->left = r->left;
			e->right = r->right;
			r_expr_free(l);
		} else if (r_expr_is_neg(r)) {
			e->type = l->type;
			e->left = l->left;
			e->right = l->right;
			r_expr_free(r);
		}
		return;
	}
}

bool r_expr_contains_symbol(struct r_expr *r, struct symbol *sym)
{
	switch (r->type) {
	case R_CONSTANT:
		return false;
	case R_VARIABLE:
		return r->left.var->sym == sym;
	case R_NOT:
		return r_expr_contains_symbol(r->left.expr, sym);
	case R_OR:
	case R_AND:
		return r_expr_contains_symbol(r->left.expr, sym) ||
		       r_expr_contains_symbol(r->right.expr, sym);
	}
	return false;
}

struct r_expr *r_expr_copy(struct r_expr *r)
{
	struct variable *var;
	struct r_expr *new = malloc(sizeof(struct r_expr));
	new->type = r->type;

	switch (r->type) {
	case R_CONSTANT:;
		var = malloc(sizeof(struct variable));
		var->on = r->left.var->on;
		new->left.var = var;
		break;
	case R_VARIABLE:;
		var = malloc(sizeof(struct variable));
		var->sym = r->left.var->sym;
		var->tri = r->left.var->tri;
		var->on = r->left.var->on;
		new->left.var = var;
		break;
	case R_NOT:
		new->left.expr = r_expr_copy(r->left.expr);
		break;
	case R_OR:
	case R_AND:
		new->left.expr = r_expr_copy(r->left.expr);
		new->right.expr = r_expr_copy(r->right.expr);
		break;
	}
	return new;
}

void r_expr_free(struct r_expr *r)
{
	switch (r->type) {
	case R_CONSTANT:
	case R_VARIABLE:;
		free(r->left.var);
		free(r);
		break;
	case R_NOT:
		r_expr_free(r->left.expr);
		free(r);
		break;
	case R_OR:
	case R_AND:
		r_expr_free(r->left.expr);
		r_expr_free(r->right.expr);
		free(r);
		break;
	}
}

gchar *r_expr_to_str(struct r_expr *r)
{
	gchar *s, *s2, *s3;

	switch (r->type) {
	case R_CONSTANT:
		s = g_strdup((r->left.var->on) ? "true" : "false");
		break;
	case R_VARIABLE:;
		char *name = r->left.var->sym->name;
		tristate tri = r->left.var->tri;
		bool on = r->left.var->on;
		s = g_strconcat(
			name,
			(on) ? " == " : " != ",
			(tri == no) ? "no" : (tri == mod) ? "mod" : "yes",
			NULL
		);
		break;
	case R_NOT:
		s2 = r_expr_to_str(r->left.expr);
		if (r->left.expr->type == R_VARIABLE) {
			s = g_strconcat("!(", s2, ")", NULL);
		} else {
			s = g_strconcat("!", s2, NULL);
		}
		g_free(s2);
		break;
	case R_OR:
		s2 = r_expr_to_str(r->left.expr);
		s3 = r_expr_to_str(r->right.expr);
		s = g_strconcat("(", s2, " || ", s3, ")", NULL);
		g_free(s2);
		g_free(s3);
		break;
	case R_AND:
		s2 = r_expr_to_str(r->left.expr);
		s3 = r_expr_to_str(r->right.expr);
		s = g_strconcat("(", s2, " && ", s3, ")", NULL);
		g_free(s2);
		g_free(s3);
		break;
	default:
		fprintf(stderr, "Unhandled type: %i\n", r->type);
	}
	return s;
}
