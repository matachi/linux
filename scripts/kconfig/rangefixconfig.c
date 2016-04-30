#include <stdlib.h>
#include <string.h>
#include "rangefix.h"

static bool print_time = true;
static bool print_iterations = true;

bool valid_symbol(struct symbol *sym)
{
	return (sym->type == S_BOOLEAN || sym->type == S_TRISTATE) &&
	        sym->name && sym_has_prompt(sym) && sym->curr.tri == 0;
}

void print_random_symbols(unsigned int num, char *kconfig)
{
	unsigned int i, k;
	unsigned int j = 0;
	struct symbol *sym;
	unsigned int number_of_symbols = 0;
	struct symbol **symbols;
	unsigned int random_nums[num];

	conf_parse(kconfig);
	conf_read(NULL);
	srand(time(NULL));

	for_all_symbols(i, sym) {
		if (valid_symbol(sym)) ++number_of_symbols;
	}
	symbols = malloc(number_of_symbols * sizeof(struct symbol *));
	for_all_symbols(i, sym) {
		if (valid_symbol(sym)) symbols[j++] = sym;
	}

	j = 0;
	while (j < num) {
		i = rand() % number_of_symbols;
		for (k = 0; k < j; ++k) {
			if (random_nums[k] == i) break;
		}
		if (k == j) random_nums[j++] = i;
	}

	for (i = 0; i < num; ++i) {
		printf("%s\n", symbols[random_nums[i]]->name);
	}

	free(symbols);
}

int main(int ac, char** av)
{
	gint64 time_total, time_total_start, time_total_end, time_setup,
		time_setup_start, time_setup_end;
	GArray *diagnoses, *diagnosis;
	int i, j;
	int option = 1;
	const char *kconfig_file = "Kconfig";
	const char *config_file = ".config";
	const char *option_name, *val;

	time_total_start = g_get_monotonic_time();

	if (strcmp(av[2], "random") == 0) {
		print_random_symbols(atoi(av[3]), kconfig_file);
		return EXIT_SUCCESS;
	}

	if (ac < 3) {
		fprintf(stderr,
		        "Both option name and value must be specified.\n");
		return EXIT_FAILURE;
	}

	if (option < ac - 2)
		kconfig_file = av[option++];

	if (option < ac - 2)
		config_file = av[option++];

	option_name = av[option++];
	val = av[option++];

	time_setup_start = g_get_monotonic_time();
	rangefix_init(kconfig_file, config_file, true);
	time_setup_end = g_get_monotonic_time();
	time_setup = time_setup_end - time_setup_start;


	diagnoses = rangefix_run_str(option_name, val);
	for (i = 0; i < diagnoses->len; ++i) {
		diagnosis = g_array_index(diagnoses, GArray *, i);
		printf("Diagnosis: ");
		for (j = 0; j < diagnosis->len; ++j) {
			printf(
				"%s, ",
				g_array_index(
					diagnosis, struct symbol *, j)->name);
		}
		printf("\n");
	}

	time_total_end = g_get_monotonic_time();
	time_total = time_total_end - time_total_start;

	if (print_time) {
		printf("Total: %i ms\n", time_total / 1000);
		printf("Setup: %i ms\n", time_setup / 1000);
		printf("Find: %i ms\n", time_find_diagnoses / 1000);
		printf("Simplify: %i ms\n", time_simplify_diagnoses / 1000);
	}
	if (print_iterations) {
		printf("Iterations: %i\n", iterations);
	}

	return EXIT_SUCCESS;
}
