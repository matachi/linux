#include <stdlib.h>
#include "expr.h"
#include "rangefixexpr.h"

/* struct r_expr *expr_to_r_expr(struct expr *expr) */

static struct r_expr *new_r_expr_var(struct symbol *sym, tristate tri, bool on)
{
	struct r_expr *r = malloc(sizeof(struct r_expr));
	r->type = R_VARIABLE;
	r->left.var = malloc(sizeof(struct variable));
	r->left.var->sym = sym;
	r->left.var->tri = tri;
	r->left.var->on = on;
	return r;
}

static struct r_expr *new_r_expr_not(struct r_expr *r)
{
	struct r_expr *not = malloc(sizeof(struct r_expr));
	not->type = R_NOT;
	not->left.expr = r;
	return not;
}

static struct r_expr *new_r_expr_and(struct r_expr *l, struct r_expr *r)
{
	struct r_expr *and = malloc(sizeof(struct r_expr));
	and->type = R_AND;
	and->left.expr = l;
	and->right.expr = r;
	return and;
}

static struct r_expr *new_r_expr_or(struct r_expr *l, struct r_expr *r)
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
		r = new_r_expr_var(expr->left.sym, lower, true);
		if (upper - lower >= 1) {
			r = new_r_expr_or(
				r,
				new_r_expr_var(expr->left.sym, lower + 1, true)
			);
		}
		if (upper - lower >= 2) {
			r = new_r_expr_or(
				r,
				new_r_expr_var(expr->left.sym, lower + 2, true)
			);
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
	return new_r_expr_or(
		new_r_expr_not(
			new_r_expr_and(
				new_r_expr_var(prop->sym, yes, true),
				expr_to_r_expr_same(prop->visible.expr, yes)
			)
		),
		expr_to_r_expr_same(prop->expr, yes)
	);	
}

static struct r_expr *select_to_constraint_m(struct property *prop)
{
	return new_r_expr_or(
		new_r_expr_not(
			new_r_expr_and(
				new_r_expr_or(
					new_r_expr_var(prop->sym, mod, true),
					new_r_expr_var(prop->sym, yes, true)
				),
				expr_to_r_expr(prop->visible.expr, mod, yes)
			)
		),
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

gchar *r_expr_to_str(struct r_expr *r)
{
	gchar *s, *s2, *s3;

	switch (r->type) {
	case R_VARIABLE:
		;
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
