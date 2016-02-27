cdef extern from "expr.h":
    struct symbol:
        char *name
    struct menu:
        menu *next
        menu *parent
        menu *list
        symbol *sym

cdef public void grail(menu *m):
    print m.next.next.next.next.next.next.next.next.sym.name

