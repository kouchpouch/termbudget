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

/* 
 * Returns all budget categories' line numbers that match the given month,
 * year. Return struct must be free'd.
 *
 * REPLACED by get_budget_catg_by_date which returns byte offsets instead of
 * line numbers.
 */
[[deprecated("Replaced by get_budget_catg_by_date")]]
struct FlexArr *get_budget_catg_by_date_ln(int month, int year) {
	struct FlexArr *pfa = 
		malloc((sizeof(struct FlexArr)) + (sizeof(long) * REALLOC_INCR));

	if (pfa == NULL) {
		memory_allocate_fail();
	}

	FILE *fptr = open_budget_csv("r");

	pfa->lines = 0;
	unsigned int realloc_counter = 0;

	seek_beyond_header(fptr);
	long linenumber = 1;
	char linebuff[LINE_BUFFER];
	char *str;
	int m;
	int y;

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		m = atoi(strsep(&str, ","));
		y = atoi(strsep(&str, ","));

		if (y == year && m == month) {
			if (realloc_counter >= REALLOC_INCR - 1) {
				realloc_counter = 0;
				struct FlexArr *tmp = realloc(pfa, (sizeof(struct FlexArr)) +
								  (REALLOC_INCR + pfa->lines) * sizeof(long));
				if (tmp == NULL) {
					free(pfa);
					memory_allocate_fail();
				}
				pfa = tmp;
			}
			pfa->data[pfa->lines] = linenumber;
			pfa->lines++;
			realloc_counter++;
		}
	}

	return pfa;
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
