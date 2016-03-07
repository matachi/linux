#include <locale.h>
#include <glib.h>
#include <stdlib.h>
#include "rangefix.h"
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

static struct symbol **garray_to_struct(GArray *array)
{
	struct symbol **output;
	int len, i;

	len = array->len * sizeof(struct symbol *);
	output = (struct symbol **) malloc(len);
	for (i = 0; i < array->len; ++i)
		output[i] = g_array_index(array, struct symbol *, i);
	return output;
}

static void print_array(char *title, GArray *array)
{
	unsigned int i;
	DEBUG("%s: [", title);
	for (i = 0; i < array->len; ++i) {
		DEBUG("%s", g_array_index(array, struct symbol *, i)->name);
		if (i < array->len - 1)
			DEBUG(", ");
		else
			DEBUG("]\n");
	}
}

GArray *rangefix_generate_diagnoses(void)
{
	unsigned int i, j, k, diagnos_index;
	struct symbol *sym, *x;
	GArray *C = g_array_new(false, false, sizeof(struct symbol *));
	GArray *E = g_array_new(false, false, sizeof(GArray *));
	GArray *R = g_array_new(false, false, sizeof(GArray *));
	GArray *e, *X, *E1, *E2, *x_set, *E_R_union;

	/* Create constraint set C */
	for_all_symbols(i, sym) {
		if (sym->flags & SYMBOL_DEF_USER) {
			C = g_array_append_val(C, sym);
		}
	}

	/* Init E with an empty diagnosis */
	GArray *empty_diagnosis = g_array_new(
		false, false, sizeof(struct symbol *));
	E = g_array_append_val(E, empty_diagnosis);

	GRand *rand = g_rand_new();
	while (E->len > 0) {
		/* A random diagnosis in E */
		diagnos_index = g_rand_int_range(rand, 0, E->len);
		GArray *E0 = g_array_index(E, GArray *, diagnos_index);

		satconfig_push();

		/* Set constraints C\E0 */
		GArray *c = set_difference(C, E0);
		print_array("Set configuration", c);
		/* struct symbol *s = extract_sym("MODULES"); */
		/* c = g_array_append_val(c, s); */
		struct symbol **symbols = garray_to_struct(c);
		satconfig_set_symbols(symbols, c->len);
		free(symbols);
		g_array_free(c, false);

		int a = satconfig_sat();

		/* Check if it is satisfiable */
		if (a == SATCONFIG_SATISFIABLE) {
			DEBUG("Satisfiable\n");
			E = g_array_remove_index(E, diagnos_index);
			R = g_array_append_val(R, E0);
			continue;
		}
		DEBUG("Unatisfiable\n");

		/* Get unsatisfiable core */
		X = satconfig_get_core();
		print_array("Got core", X);

		satconfig_pop();

		for (i = 0; i < E->len; ++i) {
			/* Get a partial diagnosis */
			e = g_array_index(E, GArray *, i);

			/* If there's already an intersection between the core
			 * and the partial diagnosis, there's nothing more to
			 * be done. */
			if (has_intersection(X, e))
				continue;

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
				E_R_union = set_union(E, R);
				g_array_remove_index(E_R_union, i);

				bool E2_subset_of_E1 = false;

				/* E" ∈ (E\e) ∪ R */
				for (k = 0; k < E_R_union->len; ++k) {
					E2 = g_array_index(
						E_R_union, GArray *, k);

					/* E" ⊆ E' */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
						break;
					}
				}

				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1) {
					/* E = E ∪ {E'} */
					E = g_array_append_val(E, E1);
				}

			}

			g_array_free(e, false);
			g_array_remove_index(E, i);
			--i;
		}

		g_array_free(X, false);
	}

	DEBUG("R length: %i\n", R->len);
	DEBUG("E length: %i\n", E->len);

	for (i = 0; i < R->len; ++i) {
		print_array("Diagnos", g_array_index(R, GArray *, i));
	}

	g_array_free(C, false);
	g_array_free(E, false);

	return R;
}

int rangefix_init(const char *kconfig_file, const char *config)
{
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	satconfig_init(kconfig_file, config, false);
	conf_read_simple(config, S_DEF_USER);
	conf_read_simple(config, S_DEF_SAT);

	return 1;
}

int rangefix_run(const char *config, tristate val)
{
	struct symbol *sym = extract_sym(config);
	rangefix_generate_diagnoses();
	return 1;
}
