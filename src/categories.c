#include "categories.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

double get_expenditures_per_category(struct BudgetTokens *bt) {
	double total = 0;
	Vec *pi = get_records_by_any(bt->m, -1, bt->y, bt->catg, NULL, TT_INCOME, -1, NULL);
	Vec *pe = get_records_by_any(bt->m, -1, bt->y, bt->catg, NULL, TT_EXPENSE, -1, NULL);
	for (size_t i = 0; i < pi->size; i++) {
		total += get_amount(pi->data[i]);
	}
	for (size_t i = 0; i < pe->size; i++) {
		total -= get_amount(pe->data[i]);
	}
	free(pe);
	free(pi);
	return total;
}
