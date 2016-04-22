#include <locale.h>
#include <glib.h>
#include <stdlib.h>
#include "rangefix.h"
#include "rangefixexpr.h"
#include "satconf.h"

#define CONFIG_DEBUG 0

#if CONFIG_DEBUG
#define DEBUG(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

static struct symbol *extract_sym(const char *config)
{
	unsigned int i;
	struct symbol *sym;

	for_all_symbols(i, sym)
		if (g_strcmp0(sym->name, config) == 0)
			return sym;
	return NULL;
}

static tristate extract_tristate(const char *val)
{
	unsigned int i;

	val = g_ascii_strdown(val, -1);
	if (g_strcmp0(val, "no") == 0)
		return no;
	else if (g_strcmp0(val, "mod") == 0)
		return mod;
	else if (g_strcmp0(val, "yes") == 0)
		return yes;
	return -1;
}

static GArray *set_difference(GArray *set1, GArray *set2)
{
	int i = 0, j;
	struct symbol *x, *y;
	GArray *output;

	output = g_array_new(false, false, sizeof(struct symbol *));
next:
	for (; i < set1->len; ++i) {
		x = g_array_index(set1, struct symbol *, i);
		for (j = 0; j < set2->len; ++j) {
			y = g_array_index(set2, struct symbol *, j);
			if (x == y) {
				++i;
				goto next;
			}
		}
		output = g_array_append_val(output, x);
	}
	return output;
}

static bool has_intersection(GArray *set1, GArray *set2)
{
	int i, j;
	struct symbol *x, *y;

	for (i = 0; i < set1->len; ++i) {
		x = g_array_index(set1, struct symbol *, i);
		for (j = 0; j < set2->len; ++j) {
			y = g_array_index(set2, struct symbol *, j);
			if (x == y) {
				return true;
			}
		}
	}
	return false;
}

static GArray *set_union(GArray *set1, GArray *set2)
{
	int i, j;
	struct symbol *x, *y;
	GArray *output;

	output = g_array_new(false, false, sizeof(void *));
	for (i = 0; i < set1->len; ++i) {
		x = g_array_index(set1, void *, i);
		output = g_array_append_val(output, x);
	}
	i = 0;
next:
	for (; i < set2->len; ++i) {
		x = g_array_index(set2, void *, i);
		for (j = 0; j < output->len; ++j) {
			y = g_array_index(output, void *, j);
			if (x == y) {
				++i;
				goto next;
			}
		}
		output = g_array_append_val(output, x);
	}
	return output;
}

static bool is_subset_of(GArray *set1, GArray *set2)
{
	int i, j;
	struct symbol *x, *y;
	bool x_in_set2;

	for (i = 0; i < set1->len; ++i) {
		x = g_array_index(set1, struct symbol *, i);
		x_in_set2 = false;
		for (j = 0; j < set2->len; ++j) {
			y = g_array_index(set2, struct symbol *, j);
			if (x == y) {
				x_in_set2 = true;
				break;
			}
		}
		if (!x_in_set2) {
			return false;
		}
	}
	return true;
}

static GArray *clone_array(GArray *array)
{
	unsigned int i;
	GArray *output;

	output = g_array_new(false, false, sizeof(void *));
	for (i = 0; i < array->len; ++i)
		g_array_append_val(output, g_array_index(array, void *, i));
	return output;
}

static GArray *clone_constraints(GArray *array)
{
	unsigned int i;
	struct r_expr *e;
	GArray *output;

	output = g_array_new(false, false, sizeof(struct r_expr *));
	for (i = 0; i < array->len; ++i) {
		e = r_expr_copy(g_array_index(array, struct r_expr *, i));
		g_array_append_val(output, e);
	}
	return output;
}

static void print_array(char *title, GArray *array)
{
	unsigned int i;
	struct symbol *sym;
	DEBUG("%s: [", title);
	for (i = 0; i < array->len; ++i) {
		sym = g_array_index(array, struct symbol *, i);
		if (sym->name != NULL)
			DEBUG("%s", sym->name);
		else
			DEBUG("NO_NAME");
		if (i < array->len - 1)
			DEBUG(", ");
	}
	DEBUG("]\n");
}

/* Remove the proposal itself from the diagnoses set */
static void remove_self(GArray *diagnoses, GArray *proposal)
{
	unsigned int i, j;
	GArray *diagnosis, *temp;

	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis = g_array_index(diagnoses, GArray *, i);
		if (diagnosis->len != proposal->len)
			continue;
		temp = set_union(diagnosis, proposal);
		if (diagnosis->len == temp->len) {
			g_array_remove_index(diagnoses, i);
			--i;
			g_array_free(diagnosis, false);
		}
		g_array_free(temp, false);
	}
}

/* Remove duplicate diagnoses */
static void remove_duplicates(GArray *diagnoses)
{
	unsigned int i, j;
	GArray *diagnosis1, *diagnosis2, *temp;

	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis1 = g_array_index(diagnoses, GArray *, i);
		for (j = i + 1; j < diagnoses->len; ++j) {
			diagnosis2 = g_array_index(diagnoses, GArray *, j);
			if (diagnosis1->len != diagnosis2->len)
				continue;
			temp = set_union(diagnosis1, diagnosis2);
			if (diagnosis1->len == temp->len) {
				g_array_remove_index(diagnoses, j);
				g_array_free(diagnosis2, false);
				--j;
			}
			g_array_free(temp, false);
		}
	}
}

GArray *rangefix_generate_diagnoses(void)
{
	unsigned int i, j, k, diagnosis_index;
	struct symbol *sym, *x;
	GArray *C = g_array_new(false, false, sizeof(struct symbol *));
	GArray *E = g_array_new(false, false, sizeof(GArray *));
	GArray *R = g_array_new(false, false, sizeof(GArray *));
	GArray *e, *X, *E1, *E2, *x_set, *E_R_union, *E_copy;

	/* Create constraint set C */
	for_all_symbols(i, sym) {
		if ((sym->type == S_BOOLEAN || sym->type == S_TRISTATE) &&
		    sym->name) {
			/* Include all bools and tristates that have names in
			 * the configuration. These are all supported symbols
			 * that the user may specify in the .config file.
			 */
			C = g_array_append_val(C, sym);
		}
	}

	/* Init E with an empty diagnosis */
	GArray *empty_diagnosis = g_array_new(
		false, false, sizeof(struct symbol *));
	E = g_array_append_val(E, empty_diagnosis);

	DEBUG("===== Generating diagnoses =====\n");

	GRand *rand = g_rand_new();
	while (E->len > 0) {
		/* A random partial diagnosis in E */
		diagnosis_index = g_rand_int_range(rand, 0, E->len);
		GArray *E0 = g_array_index(E, GArray *, diagnosis_index);

		print_array("Select partial diagnosis", E0);

		/* Set constraints C\E0 */
		GArray *c = set_difference(C, E0);
		print_array("Soft constraints", c);
		/* struct symbol *s = extract_sym("MODULES"); */
		/* c = g_array_append_val(c, s); */
		satconfig_set_symbols(c);

		switch (satconfig_sat()) {
		case SATCONFIG_SATISFIABLE:
			DEBUG("Satisfiable\n");
			print_array("Found diagnosis", E0);

			E0 = satconfig_minimize_diagnosis(c, E0);
			print_array("Simplified diagnosis", E0);

			E = g_array_remove_index(E, diagnosis_index);
			if (E0->len > 0)
				R = g_array_append_val(R, E0);
			else
				g_array_free(E0, false);

			g_array_free(c, false);
			DEBUG("\n");
			continue;
		case SATCONFIG_UNSATISFIABLE:
			DEBUG("Unsatisfiable\n");
			break;
		case SATCONFIG_UNKNOWN:
			DEBUG("Unknown\n");
			break;
		}

		/* Get unsatisfiable core */
		X = satconfig_get_core();
		print_array("Got core", X);

		E_copy = clone_array(E);
		for (i = 0; i < E_copy->len; ++i) {
			/* Get a partial diagnosis */
			e = g_array_index(E_copy, GArray *, i);
			print_array("Look at partial diagnosis", e);

			/* If there's already an intersection between the core
			 * and the partial diagnosis, there's nothing more to
			 * be done. */
			if (has_intersection(X, e)) {
				DEBUG("Intersection with core.\n");
				continue;
			}

			/* For each symbol in the core */
			for (j = 0; j < X->len; ++j) {
				x = g_array_index(X, struct symbol *, j);

				/* Create {x} */
				x_set = g_array_new(
					false, false, sizeof(struct symbol *));
				x_set = g_array_append_val(x_set, x);

				/* Create E' = e ∪ {x} */
				E1 = set_union(e, x_set);

				/* Create (E\e) ∪ R */
				E_R_union = set_union(E_copy, R);
				g_array_remove_index(E_R_union, i);

				bool E2_subset_of_E1 = false;

				/* E" ∈ (E\e) ∪ R */
				for (k = 0; k < E_R_union->len; ++k) {
					E2 = g_array_index(
						E_R_union, GArray *, k);

					/* E" ⊆ E' */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
						print_array("E\"", E2);
						print_array("E'", E1);
						break;
					}
				}

				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1) {
					/* E = E ∪ {E'} */
					E = g_array_append_val(E, E1);
					print_array(
						"Add partial diagnosis", E1);
				}

			}

			print_array("Remove partial diagnosis", e);
			g_array_free(e, false);
			g_array_remove_index(E, i);
			g_array_remove_index(E_copy, i);
			--i;
		}

		g_array_free(E_copy, false);
		g_array_free(X, false);
		g_array_free(c, false);

		DEBUG("\n");
	}

	DEBUG("R length: %i\n", R->len);
	DEBUG("E length: %i\n", E->len);

	g_array_free(C, false);
	g_array_free(E, false);

	remove_duplicates(R);

	for (i = 0; i < R->len; ++i) {
		print_array("Diagnosis", g_array_index(R, GArray *, i));
	}

	return R;
}

static void print_expr(struct expr *expr)
{
	struct gstr str = str_new();
	expr_gstr_print(expr, &str);
	DEBUG("%s\n", str_get(&str));
	str_free(&str);
}

GArray *rangefix_get_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	struct property *prop;
	GArray *constraints;
	struct r_expr *expr[2];

	constraints = g_array_new(false, false, sizeof(struct r_expr *));

	DEBUG("===== Getting constraints =====\n");

	for_all_symbols(i, sym) {
		if (sym->type != S_BOOLEAN && sym->type != S_TRISTATE)
			continue;
		DEBUG("Look at symbol %s\n", sym->name);
		for (prop = sym->prop; prop; prop = prop->next) {
			expr[0] = NULL;
			expr[1] = NULL;
			switch (prop->type) {
			case P_PROMPT:
				expr[0] = prompt_to_constraint(prop, yes);
				expr[1] = prompt_to_constraint(prop, mod);
				break;
			case P_SELECT:
				expr[0] = select_to_constraint(prop, yes);
				expr[1] = select_to_constraint(prop, mod);
				break;
			}
			if (expr[0] != NULL)
				constraints = g_array_append_val(
					constraints, expr[0]);
			if (expr[1] != NULL)
				constraints = g_array_append_val(
					constraints, expr[1]);
		}
		DEBUG("\n");
	}

	return constraints;
}

struct r_expr *rangefix_to_one_constraint(GArray *constraints)
{
	unsigned int i;
	struct r_expr *expr, *new_expr;

	if (constraints->len == 0)
		return NULL;
	if (constraints->len == 1) {
		expr = g_array_index(constraints, struct r_expr *, 0);
		g_array_free(constraints, false);
		return expr;
	}

	expr = g_array_index(constraints, struct r_expr *, 0);
	for (i = 1; i < constraints->len; ++i) {
		new_expr = malloc(sizeof(struct r_expr));
		new_expr->type = R_AND;
		new_expr->left.expr = expr;
		new_expr->right.expr = g_array_index(
			constraints, struct r_expr *, i);
		expr = new_expr;
	}
	g_array_free(constraints, false);
	return expr;
}

static void replace_symbols(struct r_expr *expr, GArray *diagnosis)
{
	switch (expr->type) {
	case R_VARIABLE:
		{
			unsigned int i;
			struct symbol *sym;
			for (i = 0; i < diagnosis->len; ++i) {
				sym = g_array_index(
					diagnosis, struct symbol *, i);
				if (sym == expr->left.var->sym)
					return;
			}
			expr->type = R_CONSTANT;
			expr->left.var->on =
				expr->left.var->on ==
				expr->left.var->sym->curr.tri ==
				expr->left.var->tri;
		}
		break;
	case R_AND:
	case R_OR:
		replace_symbols(expr->left.expr, diagnosis);
		replace_symbols(expr->right.expr, diagnosis);
		break;
	case R_NOT:
		replace_symbols(expr->left.expr, diagnosis);
		break;
	}
}

struct r_expr *rangefix_get_modified_constraint(
	struct r_expr *constraint, GArray *diagnosis
) {
	unsigned int i;
	struct expr *expr;
	struct symbol *sym;

	DEBUG("===== Getting modified constraints =====\n");

	replace_symbols(constraint, diagnosis);
	return constraint;
}

GArray *remove_constraints(GArray *constraints, GArray *diagnosis)
{
	unsigned int i, j;
	struct r_expr *expr;
	struct symbol *sym;
	bool constraint_contains_diagnosis;

	for (i = 0; i < constraints->len; ++i) {
		expr = g_array_index(constraints, struct r_expr *, i);
		constraint_contains_diagnosis = false;
		for (j = 0; j < diagnosis->len; ++j) {
			sym = g_array_index(diagnosis, struct symbol *, j);
			if (r_expr_contains_symbol(expr, sym)) {
				constraint_contains_diagnosis = true;
				break;
			}
		}
		if (!constraint_contains_diagnosis) {
			r_expr_free(expr);
			g_array_remove_index(constraints, i);
			--i;
		}
	}
	return constraints;
}

static struct r_expr *get_fix(GArray *constraints, GArray *diagnosis)
{
	unsigned int i;
	struct r_expr *constraint, *c_temp1, *c_temp2;

	constraints = remove_constraints(constraints, diagnosis);
	for (i = 0; i < constraints->len; ++i) {
		constraint = g_array_index(constraints, struct r_expr *, i);
		c_temp1 = r_expr_copy(constraint);
		rangefix_get_modified_constraint(constraint, diagnosis);
		c_temp2 = r_expr_copy(constraint);
		simplify_r_expr(constraint);
		if (constraint->type == R_CONSTANT) {
			r_expr_free(constraint);
			g_array_remove_index(constraints, i);
			--i;
			continue;
		}
		printf("Constraint: %s\n", r_expr_to_str(c_temp1));
		r_expr_free(c_temp1);
		printf("Modified constraint: %s\n", r_expr_to_str(c_temp2));
		r_expr_free(c_temp2);
		printf("Simplified constraint: %s\n\n", r_expr_to_str(constraint));
	}
	constraint = rangefix_to_one_constraint(constraints);
	return constraint;
}

GArray *rangefix_get_fixes()
{
	unsigned int i;
	GArray *constraints, *constraints2, *diagnoses, *diagnosis, *fixes;
	struct r_expr *constraint, *modified_constraint, *fix;

	diagnoses = rangefix_generate_diagnoses();
	constraints = rangefix_get_constraints();

	fixes = g_array_new(false, false, sizeof(struct r_expr *));

	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis = g_array_index(diagnoses, GArray *, i);

		fix = get_fix(clone_constraints(constraints), diagnosis);
		fixes = g_array_append_val(fixes, fix);
	}

	return fixes;
}

int rangefix_init(const char *kconfig_file, const char *config)
{
	unsigned int i;
	struct symbol *sym;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	satconfig_init(kconfig_file, config, false);

	for_all_symbols(i, sym) {
		if (sym->flags & SYMBOL_DEF_USER)
			sym->curr = sym->def[S_DEF_USER];
	}

	return EXIT_SUCCESS;
}

int rangefix_run(const char *config, const char *val)
{
	unsigned int i, j;
	struct symbol *sym;
	tristate tri;
	GArray *diagnoses, *diagnosis, *proposal;

	sym = extract_sym(config);
	if (sym == NULL) {
		fprintf(stderr, "Unknown config name.\n.");
		return EXIT_FAILURE;
	}
	sym->flags |= SYMBOL_DEF_USER;
	sym->flags |= SYMBOL_SAT;
	tri = extract_tristate(val);
	if (tri == -1) {
		fprintf(stderr, "Unknown tristate value.\n.");
		return EXIT_FAILURE;
	}
	sym->curr.tri = tri;

	diagnoses = rangefix_generate_diagnoses();

	proposal = g_array_new(false, false, sizeof(struct symbol *));
	g_array_append_val(proposal, sym);
	remove_self(diagnoses, proposal);

	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis = g_array_index(diagnoses, GArray *, i);
		for (j = 0; j < diagnosis->len; ++j) {
			printf(
				"%s, ",
				g_array_index(
					diagnosis, struct symbol *, j)->name);
		}
		printf("\n");
	}

	return EXIT_SUCCESS;
}
