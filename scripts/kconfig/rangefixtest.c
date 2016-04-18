#include <check.h>
#include <glib.h>
#include <stdlib.h>
#include "rangefixexpr.h"
#include "rangefix.h"

#define test_data(str) "scripts/kconfig/test_data/" str

static void assert_diagnosis_present(GArray *diagnoses, unsigned int n, ...);
static void assert_constraint_present(GArray *constraints, char *constraint);

START_TEST(test_load_config)
{
	ck_assert_int_eq(
		rangefix_init(test_data("Kconfig1"), NULL),
		EXIT_SUCCESS);
	struct menu *m = &rootmenu;
	ck_assert(m);
	m = m->list;
	ck_assert_str_eq(m->sym->name, "MODULES");
	m = m->next;
	ck_assert_str_eq(m->sym->name, "JBD2");
	m = m->next;
	ck_assert_str_eq(m->sym->name, "EXT4_FS");
	ck_assert(!m->next);
}
END_TEST

START_TEST(test_generate_diagnoses)
{
	/* There are 4 minimal unsatisfiable cores in this test:
	 *
	 * - [64BIT, DRIVER1]
	 * - [DRIVER2, DRIVER4, DRIVER5]
	 * - [DRIVER3, DRIVER5]
	 *
	 * A diagnosis covers at least one variable in each core. In this case
	 * there are 6 minimal diagnoses that satisfy this. */
	GArray *diagnoses, *diagnosis;

	rangefix_init(test_data("Kconfig3"), test_data("dotconfig3"));
	diagnoses = rangefix_generate_diagnoses();

	ck_assert_int_eq(diagnoses->len, 6);
	assert_diagnosis_present(diagnoses, 3, "64BIT", "DRIVER2", "DRIVER3");
	assert_diagnosis_present(diagnoses, 3, "64BIT", "DRIVER3", "DRIVER4");
	assert_diagnosis_present(diagnoses, 2, "64BIT", "DRIVER5");
	assert_diagnosis_present(diagnoses, 3, "DRIVER1", "DRIVER2", "DRIVER3");
	assert_diagnosis_present(diagnoses, 3, "DRIVER1", "DRIVER3", "DRIVER4");
	assert_diagnosis_present(diagnoses, 2, "DRIVER1", "DRIVER5");
}
END_TEST

START_TEST(test_get_constraints)
{
	/* The Kconfig file contains 4 attributes that give rise to logical
	 * constraints:
	 *
	 * - B has a prompt and 2 selects.
	 * - C has a prompt.
	 *
	 * The following config option does not give rise to a constraint:
	 *
	 * - D's prompt is always visible.
	 */
	GArray *constraints;
	unsigned int i;
	struct r_expr *expr;

	rangefix_init(test_data("Kconfig2"), test_data("dotconfig2"));
	constraints = rangefix_get_constraints();

	ck_assert_int_eq(constraints->len, 8);

	/* B depends on E */
	assert_constraint_present(
		constraints, "(B != yes || E == yes)");
	assert_constraint_present(
		constraints, "(B != mod || E != no)");

	/* B select A */
	assert_constraint_present(
		constraints, "(!(B == yes && E == yes) || A == yes)");
	assert_constraint_present(
		constraints,
		"(!(B != no && E != no) || A != no)");

	/* B select D if A */
	assert_constraint_present(
		constraints,
		"(!(B == yes && (E == yes && A == yes)) || D == yes)");
	assert_constraint_present(
		constraints,
		"(!(B != no && (E != no && A != no)) || D != no)");

	/* C depends on A && !B */
	assert_constraint_present(
		constraints,
		"(C != yes || (A == yes && B == no))");
	assert_constraint_present(
		constraints,
		"(C != mod || (A != no && B != yes))");
}
END_TEST

START_TEST(test_get_modified_constraint)
{
	GArray *constraints, *diagnoses, *diagnosis;
	struct r_expr *constraint, *modified_constraint;
	gchar *str;

	rangefix_init(test_data("Kconfig2"), test_data("dotconfig2"));
	diagnoses = rangefix_generate_diagnoses();
	constraints = rangefix_get_constraints();
	constraint = rangefix_to_one_constraint(constraints);
	diagnosis = g_array_index(diagnoses, GArray *, 0);
	if (g_strcmp0(
		g_array_index(diagnosis, struct symbol *, 0)->name,
		"C"
	) != 0)
		diagnosis = g_array_index(diagnoses, GArray *, 1);

	modified_constraint = rangefix_get_modified_constraint(
		constraint, diagnosis);

	str = r_expr_to_str(modified_constraint);
	ck_assert_str_eq(
		str,
		"((((((((false || false) && (false || true)) && (!(false && "
		"false) || false)) && (!(true && true) || true)) && (!(false "
		"&& (false && false)) || false)) && (!(true && (true && "
		"true)) || true)) && (C != yes || (false && true))) && (C != "
		"mod || (true && false)))");
	g_free(str);
}
END_TEST

START_TEST(test_get_fixes)
{
	/* It is possible to solve the configuration in two different ways.
	 * Either by assigning n to B, or by assigning n to C.
	 */
	GArray *fixes;

	rangefix_init(test_data("Kconfig2"), test_data("dotconfig2"));
	fixes = rangefix_get_fixes();

	ck_assert_int_eq(fixes->len, 1);
	assert_constraint_present(fixes, "(C != yes && C != mod)");
}
END_TEST

START_TEST(test_r_expr)
{
	struct symbol *sym;
	struct property *prompt, *select;
	unsigned int i;
	struct r_expr *r;

	gchar *expected_select_y =
		"(!(A == yes && ((E == yes || F == yes) && "
		"(D == yes && C != mod))) || B == yes)";

	gchar *expected_select_m =
		"(!(A != no && ((E != no || F != no) && (D != no && "
		"C != mod))) || B != no)";

	gchar *expected_prompt_y =
		"(A != yes || (E == yes || F == yes))";

	gchar *expected_prompt_m =
		"(A != mod || (E != no || F != no))";

	rangefix_init(test_data("Kconfig4"), NULL);

	for_all_symbols(i, sym)
		if (g_strcmp0(sym->name, "A") == 0) {
			prompt = sym_get_prompt(sym);
			for_all_properties(sym, select, P_SELECT)
				goto found;
		}

found:
	r = select_to_constraint(select, yes);
	ck_assert_str_eq(r_expr_to_str(r), expected_select_y);

	r = select_to_constraint(select, mod);
	ck_assert_str_eq(r_expr_to_str(r), expected_select_m);

	r = prompt_to_constraint(prompt, yes);
	ck_assert_str_eq(r_expr_to_str(r), expected_prompt_y);

	r = prompt_to_constraint(prompt, mod);
	ck_assert_str_eq(r_expr_to_str(r), expected_prompt_m);
}
END_TEST

Suite *test_suite(void);
Suite *test_suite(void) {
	Suite *s = suite_create("RangeFix");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);

	tcase_add_test(tc_core, test_load_config);
	tcase_add_test(tc_core, test_generate_diagnoses);
	tcase_add_test(tc_core, test_get_constraints);
	tcase_add_test(tc_core, test_get_modified_constraint);
	tcase_add_test(tc_core, test_get_fixes);
	tcase_add_test(tc_core, test_r_expr);

	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = test_suite();
	sr = srunner_create(s);

	/* srunner_set_fork_status(sr, CK_NOFORK); */
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Check if a diagnosis is present in the diagnoses array.
 *
 * An example call:
 *     assert_diagnosis_present(diagnoses, 3, "64BIT", "DRIVER4", "DRIVER5");
 */
static void assert_diagnosis_present(GArray *diagnoses, unsigned int n, ...) {
	unsigned int i, j, k, names_in_diagnosis;
	va_list ap;
	GArray *input, *diagnosis;
	struct symbol *sym;
	char *name;

	va_start(ap, n);
	input = g_array_new(false, false, sizeof(char *));
	for (i = 0; i < n; ++i) {
		name = va_arg(ap, char *);
		input = g_array_append_val(input, name);
	}
	va_end(ap);
	assert(input->len == n);

	/* Iterare over all diagnoses */
	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis = g_array_index(diagnoses, GArray *, i);
		names_in_diagnosis = 0;
		/* Iterate over all symbols in the diagnosis */
		for (j = 0; j < diagnosis->len; ++j) {
			sym = g_array_index(diagnosis, struct symbol *, j);
			/* Iterate over the names in the input */
			for (k = 0; k < input->len; ++k) {
				name = g_array_index(input, char *, k);
				if (g_strcmp0(sym->name, name) == 0) {
					++names_in_diagnosis;
					break;
				}
			}
		}
		if (names_in_diagnosis == input->len) {
			goto deallocate;
		}
	}
	ck_assert(false);
deallocate:
	g_array_free(input, false);
}

/* Check if a constraint is present in the constraints array.
 *
 * An example call:
 *     assert_constraint_present(constraints, "!B [=n] || E [=n]");
 */
static void assert_constraint_present(GArray *constraints, char *constraint) {
	unsigned int i;
	struct r_expr *expr;
	gchar *str;

	for (i = 0; i < constraints->len; ++i) {
		expr = g_array_index(constraints, struct r_expr *, i);

		str = r_expr_to_str(expr);
		if (g_strcmp0(str, constraint) == 0) {
			g_free(str);
			return;
		}
		g_free(str);
	}
	ck_assert(false);
}
