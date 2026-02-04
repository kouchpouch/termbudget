/*
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program. If not, see <https://www.gnu.org/licenses/>. 
 */
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "main.h"
#include "tui.h"

FILE *open_file(char *mode, char *dir) {
	FILE *fptr = fopen(dir, mode);
	if (fptr == NULL) {
		perror("Failed to open file");
		exit(1);
	} else {
		return fptr;
	}
}

FILE *open_budget_csv(char *mode) {
	FILE *f = open_file(mode, BUDGET_DIR);
	return f;
}

FILE *open_record_csv(char *mode) {
	FILE *f = open_file(mode, RECORD_DIR);
	return f;
}

FILE *open_temp_csv(void) {
	FILE *tmpfptr = fopen(TEMP_FILE_DIR, "w+");
	if (tmpfptr == NULL) {
		perror("Failed to open file");
		exit(1);
	}
	return tmpfptr;
}

unsigned int boff_to_linenum(long b) {
	FILE *fptr = open_record_csv("r");
	char linebuff[LINE_BUFFER];
	int linenum = 0;
	
	while(fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		if (ftell(fptr) == b) {
			break;
		}
		linenum++;
	}

	fclose(fptr);
	
	return linenum;
}

void seek_n_fields(char **line, int n) {
	for (int i = 0; i < n; i++) {
		strsep(line, ",");
	}
}

int seek_beyond_header(FILE *fptr) {
	int i = 0;
	char c = getc(fptr);
	while (c != '\n' && c != EOF) {
		c = getc(fptr);
		i++;
	}
	if (i == 0) {
		return -1;
	} else {
		return 0;
	}
}

unsigned int get_num_fields(FILE *fptr) {
	unsigned int n = 1;
	rewind(fptr);

	char c;
	do {
		c = fgetc(fptr);
		if (c == ',') {
			n++;
		}
	} while (c != '\n' && c != EOF);

	return n;
}

void free_budget_tokens(struct BudgetTokens *pbt) {
	free(pbt->catg);
	free(pbt);
}

Vec *get_records_by_any(int month, int day, int year, char *category, 
						char *description, int transtype, double amount) 
{
	FILE *fptr = open_record_csv("r");
	Vec *prbc = malloc(sizeof(*prbc) + (sizeof(long) * REALLOC_INCR));
	if (prbc == NULL) {
		memory_allocate_fail();
	}

	prbc->size = 0;
	prbc->capacity = REALLOC_INCR;

	char linebuff[LINE_BUFFER];
	char *str;

	struct LineData ld_, *ld = &ld_;

	bool date = false;
	bool cat = false;
	bool desc = false;
	bool tt = false;
	bool amt = false;

	long tmpbo;

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		tmpbo = ftell(fptr);
		tokenize_record(ld, &str);
		date = false;
		cat = false;
		desc = false;
		tt = false;
		amt = false;
		if (year < 0 && month < 0 && day < 0) {
			date = true;
		} else if (year > 0 && month > 0 && day > 0) {
			if (year == ld->year && month == ld->month && day == ld->day) {
				date = true;
			}
		} else if (year > 0 && month > 0 && day < 0) {
			if (year == ld->year && month == ld->month) {
				date = true;
			}
		} else if (year > 0 && month < 0 && day < 0) {
			if (year == ld->year) {
				date = true;
			}
		}

		if (category == NULL) {
			cat = true;
		} else if (category != NULL) {
			if (strcmp(category, ld->category) == 0) {
				cat = true;
			}
		}

		if (description == NULL) {
			desc = true;
		} else if (description != NULL) {
			if (strcmp(description, ld->desc) == 0) {
				desc = true;
			}
		}

		if (transtype < 0) {
			tt = true;
		} else if (transtype >= 0) {
			if (transtype == ld->transtype) {
				tt = true;
			}
		}

		if (amount < 0) {
			amt = true;
		} else if (amount >= 0) {
			if (amount == ld->amount) {
				amt = true;
			}
		}

		if (date && cat && desc && tt && amt) {
			if (prbc->size >= prbc->capacity) {
				prbc->capacity += REALLOC_INCR;
				Vec *tmp = realloc(prbc, sizeof(Vec) + (sizeof(long) * prbc->capacity));
				if (tmp == NULL) {
					free(prbc);
					memory_allocate_fail();
				}
			}
			prbc->data[prbc->size] = tmpbo;
			prbc->size++;
		}
	}

	fclose(fptr);
	return prbc;
}

struct Categories *get_budget_catg_by_date(int month, int year) {
	struct Categories *pc = 
		malloc((sizeof(*pc)) + (sizeof(char *) * REALLOC_INCR));

	if (pc == NULL) {
		memory_allocate_fail();
	}

	pc->size = 0;
	pc->capacity = REALLOC_INCR;

	FILE *fptr = open_budget_csv("r");

	seek_beyond_header(fptr);
	char linebuff[LINE_BUFFER];
	char *str;
	char *catg;
	int m;
	int y;

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		m = atoi(strsep(&str, ","));
		y = atoi(strsep(&str, ","));
		catg = strsep(&str, ",");

		if (y == year && m == month) {
			if (pc->size >= pc->capacity) {
				pc->capacity += REALLOC_INCR;
				struct Categories *tmp = 
					realloc(pc, sizeof(struct Categories) + 
			 				(sizeof(char *) * pc->capacity));

				if (tmp == NULL) {
					free(pc);
					memory_allocate_fail();
				}
				pc = tmp;
			}
			pc->categories[pc->size] = strdup(catg);
			pc->size++;
		}
	}

	return pc;
}

Vec *get_budget_catg_by_date_bo(int month, int year) {
	Vec *pcbo = malloc((sizeof(*pcbo)) + (sizeof(long) * REALLOC_INCR));
	if (pcbo == NULL) {
		memory_allocate_fail();
	}

	pcbo->capacity = REALLOC_INCR;
	pcbo->size = 0;

	FILE *fptr = open_budget_csv("r");

	seek_beyond_header(fptr);
	char linebuff[LINE_BUFFER];
	char *str;
	int m;
	int y;

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		m = atoi(strsep(&str, ","));
		y = atoi(strsep(&str, ","));

		if (y == year && m == month) {
			if (pcbo->size >= pcbo->capacity) {
				pcbo->capacity += REALLOC_INCR;
				Vec *tmp = realloc(pcbo, sizeof(Vec) + (sizeof(long) * pcbo->capacity));
				if (tmp == NULL) {
					free(pcbo);
					memory_allocate_fail();
				}
				pcbo = tmp;
			}
			pcbo->data[pcbo->size] = ftell(fptr);
			pcbo->size++;
		}
	}

	return pcbo;
}

struct BudgetTokens *tokenize_budget_byte_offset(long bo) {
	if (bo == 0) {
		return NULL;
	}
	struct BudgetTokens *pbt = malloc(sizeof(*pbt));
	if (pbt == NULL) {
		memory_allocate_fail();
	}

	FILE *fptr = open_budget_csv("r");

	fseek(fptr, bo, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		free(pbt);
		return NULL;
	}

	pbt->m = atoi(strsep(&str, ","));
	pbt->y = atoi(strsep(&str, ","));
	char *tmp = strndup(strsep(&str, ","), MAX_LEN_CATG);
	if (tmp == NULL) {
		free(pbt);
		memory_allocate_fail();
	}
	pbt->catg = tmp;
	pbt->amount = atof(strsep(&str, ","));

	return pbt;
}

struct BudgetTokens *tokenize_budget_line(int line) {
	if (line == 0) {
		return NULL;
	}
	struct BudgetTokens *pbt = malloc(sizeof(struct BudgetTokens));
	if (pbt == NULL) {
		memory_allocate_fail();
	}

	FILE *fptr = open_budget_csv("r");

	rewind(fptr);
	seek_beyond_header(fptr);

	char linebuff[LINE_BUFFER];
	char *str;

	int i = 1;
	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			free(pbt);
			return NULL;
		} else if (i == line) {
			break;
		}
		i++;
	}

	fclose(fptr);

	if (i > line || i < line) {
		free(pbt);
		return NULL;
	}

	pbt->m = atoi(strsep(&str, ","));
	pbt->y = atoi(strsep(&str, ","));
	char *tmp = strndup(strsep(&str, ","), MAX_LEN_CATG);
	if (tmp == NULL) {
		free(pbt);
		memory_allocate_fail();
	}
	pbt->catg = tmp;
	pbt->amount = atof(strsep(&str, ","));

	return pbt;
}

void tokenize_record(struct LineData *ld, char **str) {
	char *token;
	for (int i = 0; i < CSV_FIELDS; i++) {
		token = strsep(str, ",");
		if (token == NULL) break;
		switch(i) {
		case 0:
			ld->month = atoi(token);
			break;
		case 1:
			ld->day = atoi(token);
			break;
		case 2:
			ld->year = atoi(token);
			break;
		case 3:
			token[strcspn(token, "\n")] = '\0';
			ld->category = token;
			break;
		case 4:
			token[strcspn(token, "\n")] = '\0';
			ld->desc = token;
			break;
		case 5:
			ld->transtype = atoi(token);
			break;
		case 6:
			ld->amount = atof(token);
			break;
		}
	}
}

struct DateMDY *get_date_mdy(int line) {
	struct DateMDY *pd = calloc(sizeof(*pd), 1);
	FILE *fptr = open_record_csv("r");

	rewind(fptr);

	char linebuff[LINE_BUFFER];
	char *str;

	int i = 0;
	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (i == line) {
			break;
		}
		i++;
	}

	fclose(fptr);

	if (i > line || i < line) {
		return NULL;
	}

	pd->m = atoi(strsep(&str, ","));
	pd->d = atoi(strsep(&str, ","));
	pd->y = atoi(strsep(&str, ","));

	return pd;
}

int get_int_field(int line, int field) {
	FILE *fptr = open_record_csv("r");

	if (field > get_num_fields(fptr) || field < 1) {
		perror("That field doesn't exist");
		fclose(fptr);
		return -1;
	}

	rewind(fptr);

	char linebuff[LINE_BUFFER];
	char *str;
	int i = 0;

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (i == line) {
			break;
		}
		i++;
	}

	if (i > line || i < line) {
		return -1;
	}

	fclose(fptr);

	seek_n_fields(&str, field - 1);

	return atoi(strsep(&str, ","));
}

Vec *index_csv(FILE *fptr) {
	Vec *pidx =
		malloc(sizeof(Vec) + (sizeof(long) * INDEX_ALLOC));
	if (pidx == NULL) {
		memory_allocate_fail();
	}
	pidx->capacity = INDEX_ALLOC;
	pidx->size = 0;

	char linebuff[LINE_BUFFER];

	while (fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		pidx->data[pidx->size] = ftell(fptr);
		pidx->size++;

		if (pidx->size >= pidx->capacity) {
			if (pidx->capacity * 2 <= MAX_ALLOC) {
				pidx->capacity *= 2;
			} else {
				pidx->capacity += MAX_ALLOC;
			}
			Vec *tmp =
				realloc(pidx, sizeof(Vec) + (sizeof(long) * pidx->capacity));
			if (tmp == NULL) {
				free(pidx);
				memory_allocate_fail();
			}
			pidx = tmp;
		}
	}

	return pidx;
}

[[deprecated("Struct FlexArr changed to a Vec")]]
struct FlexArr *index_csv_flexarr(FILE *fptr) {
	struct FlexArr *pidx =
		malloc(sizeof(struct FlexArr) + (sizeof(long) * INDEX_ALLOC));
	if (pidx == NULL) {
		memory_allocate_fail();
	}
	unsigned long capacity = INDEX_ALLOC;
	pidx->lines = 0;

	char linebuff[LINE_BUFFER];

	while (fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		pidx->data[pidx->lines] = ftell(fptr);
		pidx->lines++;

		if (pidx->lines >= capacity) {
			if (capacity * 2 <= MAX_ALLOC) {
				capacity *= 2;
			} else {
				capacity += MAX_ALLOC;
			}
			struct FlexArr *tmp =
				realloc(pidx, sizeof(struct FlexArr) + (sizeof(long) * capacity));
			if (tmp == NULL) {
				free(pidx);
				memory_allocate_fail();
			}
			pidx = tmp;
		}
	}

	return pidx;
}

// Do not use. Has been renamed to index_csv_old to prevent refactoring
[[deprecated("Old inefficient indexer, do not use. Replaced with index_csv")]]
struct FlexArr *index_csv_old(FILE *fptr) {
	struct FlexArr *pidx = 
		malloc(sizeof(struct FlexArr) + sizeof(long));
	if (pidx == NULL) {
		memory_allocate_fail();
	}

	long temp_byte_offset = ftell(fptr);
	pidx->lines = 0;
	rewind(fptr);
	//FILE *fptr = open_record_csv("r");
	char linebuff[LINE_BUFFER];

	while (1) {
		char *test = fgets(linebuff, sizeof(linebuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->lines++;
	}

	struct FlexArr *tmp = realloc(pidx, sizeof(*pidx) + (pidx->lines * sizeof(long)));

	if (tmp == NULL) {
		memory_allocate_fail();
	}
	pidx = tmp;

	rewind(fptr);

	for (int i = 0; i < pidx->lines; i++) {
		char *test = fgets(linebuff, sizeof(linebuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->data[i] = ftell(fptr);
	}

	fseek(fptr, temp_byte_offset, SEEK_SET);
	return pidx;
}
