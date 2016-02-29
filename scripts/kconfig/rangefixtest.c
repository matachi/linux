#include <stdlib.h>
#include <check.h>
#include "rangefix.h"

#define test_data(str) "scripts/kconfig/test_data/" str


START_TEST(test_load_config)
{
    ck_assert_int_eq(rangefix_init(test_data("Kconfig1")), 1);
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

Suite *test_suite(void);
Suite *test_suite(void) {
    Suite *s = suite_create("RangeFix");
    TCase *tc_core = tcase_create("Core");
    suite_add_tcase(s, tc_core);

    tcase_add_test(tc_core, test_load_config);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = test_suite();
    sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
