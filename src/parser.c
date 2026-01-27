#include "parser.h"
#include "main.h"

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

int get_int_record_field(int line, int field) {
	FILE *fptr = open_record_csv("r");
	unsigned int maxfields = get_num_fields(fptr);
	if (field > get_num_fields(fptr)) {
		perror("That field doesn't exist");
		fclose(fptr);
		return -1;
	}

	return 0;
}
