#include "parser.h"
#include "main.h"

FILE *open_record_csv(char *mode) {
	FILE *fptr = fopen(RECORD_DIR, mode);
	if (fptr == NULL) {
		perror("Failed to open file");
		exit(1);
	} else {
		return fptr;
	}
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
