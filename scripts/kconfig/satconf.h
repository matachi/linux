#ifndef SATCONF_H
#define SATCONF_H

#include <glib.h>

#define SATCONFIG_UNKNOWN         0
#define SATCONFIG_SATISFIABLE     10
#define SATCONFIG_UNSATISFIABLE   20

struct symbol;
struct symbol_lit;

struct unsat_core {
	struct unsat_core *next;
	struct symbol *sym;
};

void satconfig_init(const char *Kconfig_file, const char *config,
                    bool randomize);
struct symbol_lit *satconfig_update_symbol(struct symbol *sym);
void satconfig_update_all_symbols(void);
void satconfig_solve(void);

int satconfig_push(void);
int satconfig_pop(void);
int satconfig_sat(void);
void satconfig_set_symbols(struct symbol **symbols, int n);
GArray *satconfig_get_core(void);

#endif
