#ifndef MAIN_H
#define MAIN_H

#define REALLOC_INCR 64
#define INDEX_ALLOC 1024
#define MAX_ALLOC 1024 * 1024

#define RECORD_DIR "./data.csv"
#define RECORD_BAK_DIR "./data.csv.bak"
#define CSV_FIELDS 7
#define TEMP_FILE_DIR "./tmp"
#define BUDGET_DIR "./budget.csv"
#define BUDGET_BAK_DIR "./budget.csv.bak"
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8

#define MIN_YEAR 1000
#define MAX_YEAR 9999

#define MAX_LEN_AMOUNT 9
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4
#define MIN_INPUT_CHAR 2
#define INPUT_MSG_Y_OFFSET 2

typedef struct {
	unsigned long capacity;
	unsigned long size;
	long data[];
} Vec;

struct FlexArr {
	long lines;
	long data[];
};

struct Categories {
	unsigned long count;
	unsigned long capacity;
	char *categories[];
};

/* Exits the program with "exit(1)" and prints the error message. */
extern void memory_allocate_fail();


#endif
