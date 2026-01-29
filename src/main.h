#ifndef MAIN_H
#define MAIN_H

#define RECORD_DIR "./data.csv"
#define RECORD_BAK_DIR "./data.csv.bak"
#define CSV_FIELDS 7
#define TEMP_FILE_DIR "./tmp"
#define BUDGET_DIR "./budget.csv"
#define BUDGET_BAK_DIR "./budget.csv.bak"
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
#define REALLOC_FLAG 64

#define MIN_YEAR 1000
#define MAX_YEAR 9999

#define MAX_LEN_AMOUNT 9
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4
#define MIN_INPUT_CHAR 2
#define INPUT_MSG_Y_OFFSET 2

struct FlexArr {
	int lines;
	int data[];
};

#endif
