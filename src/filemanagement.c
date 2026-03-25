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

#include <stdio.h>
#include <stdlib.h>
#include "filemanagement.h"

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

static int move_tmp_to_main(FILE *tmp, FILE *main, char *dir, char *backdir) {
	if (fclose(main) == -1) {
		perror("Failed to close main file");
		return -1;
	} else {
		main = NULL;
	}
	if (fclose(tmp) == -1) {
		perror("Failed to close temporary file");
		return -1;
	} else {
		tmp = NULL;
	}
	if (rename(dir, backdir) == -1) {
		perror("Failed to move main file");	
		return -1;
	}
	if (rename(TEMP_FILE_DIR, dir) == -1) {
		perror("Failed to move temporary file");	
		return -1;
	}
	return 0;
}

int mv_tmp_to_budget_file(FILE *tmp, FILE* main) {
	int retval = move_tmp_to_main(tmp, main, BUDGET_DIR, BUDGET_BAK_DIR);
	return retval;
}

int mv_tmp_to_record_file(FILE *tmp, FILE* main) {
	int retval = move_tmp_to_main(tmp, main, RECORD_DIR, RECORD_BAK_DIR);
	return retval;
}
