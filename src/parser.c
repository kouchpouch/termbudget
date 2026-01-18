#include "helper.h"
#include "parser.h"

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
