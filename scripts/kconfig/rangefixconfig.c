#include <stdlib.h>
#include "rangefix.h"

int main(int ac, char** av)
{
	int option = 1;
	const char *kconfig_file = "Kconfig";
	const char *config_file = ".config";
	const char *option_name, *val;

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

	rangefix_init(kconfig_file, config_file);
	return rangefix_run(option_name, val);
}
