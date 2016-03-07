#include <check.h>
#include <glib.h>
#include <stdlib.h>
#include "rangefix.h"

#define test_data(str) "scripts/kconfig/test_data/" str


START_TEST(test_load_config)
{
	ck_assert_int_eq(rangefix_init(test_data("Kconfig1"), NULL), 1);
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
	GArray *diagnoses, *diagnos;
	struct symbol *sym;
	char *name;
	unsigned int i;

	rangefix_init(test_data("Kconfig3"), test_data("dotconfig3"));
	diagnoses = rangefix_generate_diagnoses();
	ck_assert_int_eq(diagnoses->len, 1);

	diagnos = g_array_index(diagnoses, GArray *, 0);
	for (i = 0; i < diagnos->len; ++i) {
		sym = g_array_index(diagnos, struct symbol *, i);
		name = sym->name;
		if (g_strcmp0(name, "DRIVER1") == 0 ||
		    g_strcmp0(name, "DRIVER2") == 0 ||
		    g_strcmp0(name, "DRIVER3") == 0) {
			ck_assert(true);
			continue;
		}
		ck_assert(false);
	}
}
END_TEST

START_TEST(test_run)
{
	rangefix_init(test_data("Kconfig2"), test_data("dotconfig2"));
	rangefix_run("B", yes);
}
END_TEST

Suite *test_suite(void);
Suite *test_suite(void) {
	Suite *s = suite_create("RangeFix");
	TCase *tc_core = tcase_create("Core");
	suite_add_tcase(s, tc_core);

	tcase_add_test(tc_core, test_load_config);
	tcase_add_test(tc_core, test_generate_diagnoses);
	tcase_add_test(tc_core, test_run);

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
