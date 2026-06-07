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
 *
 * termbudget 2026
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "filemanagement.h"
#include "dynamic_string.h"
#include "flags.h"

#ifndef TB_RELATIVE_DIRS

char program_dir        [PATH_MAX];
char record_dir         [PATH_MAX];
char record_bak_dir     [PATH_MAX];
char tmp_file_dir       [PATH_MAX];
char converted_file_dir [PATH_MAX];
char budget_dir         [PATH_MAX];
char budget_bak_dir     [PATH_MAX];

/* Sets all dir variables to zero */
static void init_dir_variables(void)
{
	memset(program_dir,        0, sizeof(program_dir));
	memset(record_dir,         0, sizeof(record_dir));
	memset(record_bak_dir,     0, sizeof(record_bak_dir));
	memset(tmp_file_dir,       0, sizeof(tmp_file_dir));
	memset(converted_file_dir, 0, sizeof(converted_file_dir));
	memset(budget_dir,         0, sizeof(budget_dir));
	memset(budget_bak_dir,     0, sizeof(budget_bak_dir));
}

/* Fills the dir variables with the full path plus the file name as is defined
 * by macros. Assertions for each path to verify it does not exceed PATH_MAX
 */
static void set_directories(struct d_string *p)
{
	strcat(program_dir, p->string);

	strcat(record_dir, program_dir);
	assert(strlen(record_dir) + strlen(RECORD_FILE) < PATH_MAX);
	strcat(record_dir, RECORD_FILE);

	strcat(record_bak_dir, program_dir);
	assert(strlen(record_bak_dir) + strlen(RECORD_BAK_FILE) < PATH_MAX);
	strcat(record_bak_dir, RECORD_BAK_FILE);

	strcat(tmp_file_dir, program_dir);
	assert(strlen(tmp_file_dir) + strlen(TEMP_FILE) < PATH_MAX);
	strcat(tmp_file_dir, TEMP_FILE);

	strcat(converted_file_dir, program_dir);
	assert(strlen(converted_file_dir) + strlen(CONVERTED_FILE) < PATH_MAX);
	strcat(converted_file_dir, CONVERTED_FILE);

	strcat(budget_dir, program_dir);
	assert(strlen(budget_dir) + strlen(BUDGET_FILE) < PATH_MAX);
	strcat(budget_dir, BUDGET_FILE);

	strcat(budget_bak_dir, program_dir);
	assert(strlen(budget_bak_dir) + strlen(BUDGET_BAK_FILE) < PATH_MAX);
	strcat(budget_bak_dir, BUDGET_BAK_FILE);
}

/* Prints all dir variables */
static void debug_print_directories(void)
{
	printf("%s\n", "------Program files------");
	printf("%s\n", program_dir);
	printf("%s\n", record_dir);
	printf("%s\n", record_bak_dir);
	printf("%s\n", tmp_file_dir);
	printf("%s\n", converted_file_dir);
	printf("%s\n", budget_dir);
	printf("%s\n", budget_bak_dir);
}

#endif

enum err_data_dir {
	ERR_DATA_OK = 0,
	ERR_DATA_NOEXIST,
	ERR_DATA_NOENV,
};

static bool is_root(void)
{
	char *username = getenv("USER");
	if (strcasecmp("root", username) == 0) {
		return true;
	} else if (username == NULL) {
		return true;
	} else {
		return false;
	}
}

static bool dir_exists(char *path)
{
	DIR *d = opendir(path);
	if (d == NULL) {
		perror("opendir()");
		printf("PATH: %s\n", path);
		return false;
	} else {
		closedir(d);
		return true;
	}
}

static enum err_data_dir get_dir_by_env(struct d_string *path, char *env)
{
	char *dir = getenv(env);

	if (dir == NULL) {
		if (debug_flag) {
			printf("%s $%s\n", "Could not get environment variable", env);
		}
		return ERR_DATA_NOENV;
	}

	if (!dir_exists(dir)) {
		return ERR_DATA_NOEXIST;
	} else {
		if (debug_flag) {
			printf("%s $%s\n", "Found environment variable", env);
		}
	}

	concatenate_d_string(&path, dir, strlen(dir));
	return ERR_DATA_OK;
}

/* Returns the full path to the user data directory */
static enum err_data_dir get_user_data_dir(struct d_string *path_buffer)
{
	enum err_data_dir e;
	char *append_dir = "/.local/share";

	if (is_root()) {
		puts("Do not run this program as root, exiting");
		exit(1);
	}

	if ((e = get_dir_by_env(path_buffer, "XDG_DATA_HOME")) != ERR_DATA_OK) {
		if ((e = get_dir_by_env(path_buffer, "HOME")) != ERR_DATA_OK) {
			return e;
		} else {
			concatenate_d_string(&path_buffer, append_dir, strlen(append_dir));
		}
	}

	if (!dir_exists(path_buffer->string)) {
		return ERR_DATA_NOEXIST;
	}

	return ERR_DATA_OK;
}

static int make_program_data_dir(char *full_path)
{
	errno = 0;
	if (mkdir(full_path, 0777) == -1) {
		if (errno == EEXIST) {
			printf("%s already exists\n", full_path);
			return 0;
		} else {
			perror("mkdir()");
			return -1;
		}
	}

	return 0;
}

static void handle_err_data_dir(enum err_data_dir e)
{
	switch (e) {
	case ERR_DATA_OK:
		break;

	case ERR_DATA_NOEXIST:
		break;

	case ERR_DATA_NOENV:
		printf("%s\n", "Environment variable $HOME or $XDG_DATA_HOME are not set");
		printf("%s\n", "Cannot find program data directory. Exiting");
		exit(1);
	}
}

int create_program_directory(void)
{
	struct d_string *full_path = create_d_string(PATH_MAX);
	char *subdir_name = "/termbudget";

	enum err_data_dir err = 0;
	err = get_user_data_dir(full_path);
	if (err != ERR_DATA_OK) {
		handle_err_data_dir(err);
	}

	concatenate_d_string(&full_path, subdir_name, strlen(subdir_name));

	if (!dir_exists(full_path->string)) {
		make_program_data_dir(full_path->string);
	} else {
		if (debug_flag) {
			printf("Program directory exists\n");
		}
	}

	if (debug_flag) {
		printf("Program data full path: %s\n", full_path->string);
	}

	assert(full_path->len < PATH_MAX);

#ifndef TB_RELATIVE_DIRS
	init_dir_variables();
	set_directories(full_path);
	if (debug_flag) {
		debug_print_directories();
	}
	free(full_path);
	full_path = NULL;
#else
	puts("USING RELATIVE DIRECTORIES");
#endif

	return 0;
}

/* Opens file at "dir", checks if the fopen function fails and terminates
 * program if it does. */
FILE *open_file(char *mode, char *dir)
{
	FILE *fptr = fopen(dir, mode);
	if (fptr == NULL) {
		perror(NULL);
		exit(1);
	} else {
		return fptr;
	}
}

/* Opens BUDGET_DIR with open_file() */
FILE *open_budget_csv(char *mode)
{
#ifdef TB_RELATIVE_DIRS
	FILE *f = open_file(mode, BUDGET_DIR);
#else
	FILE *f = open_file(mode, budget_dir);
#endif
	return f;
}

/* Opens RECORD_DIR with open_file() */
FILE *open_record_csv(char *mode)
{
#ifdef TB_RELATIVE_DIRS
	FILE *f = open_file(mode, RECORD_DIR);
#else
	FILE *f = open_file(mode, record_dir);
#endif
	return f;
}

/* Creates a temporary file in TEMP_FILE, opens and truncates if it already
 * exists, checks for fopen() failures. */
FILE *open_temp_csv(void)
{
#ifdef TB_RELATIVE_DIRS
	FILE *tmpfptr = fopen(TEMP_DIR, "w+");
#else
	FILE *tmpfptr = fopen(tmp_file_dir, "w+");
#endif
	if (tmpfptr == NULL) {
		perror(NULL);
		exit(1);
	}
	return tmpfptr;
}

/* Renames file "tmp" to file "main" in directory "dir" and creates a backup
 * of "main" in "backdir". */
static int move_tmp_to_main(FILE *tmp, FILE *main, char *dir, char *backdir)
{
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
#ifdef TB_RELATIVE_DIRS
	if (rename(TEMP_DIR, dir) == -1) {
#else
	if (rename(tmp_file_dir, dir) == -1) {
#endif
		perror("Failed to move temporary file");	
		return -1;
	}
	return 0;
}

/* Calls move_tmp_to_main with "dir" as BUDGET_DIR and "backdir" as
 * BUDGET_BAK_DIR */
int mv_tmp_to_budget_file(FILE *tmp, FILE* main)
{
#ifdef TB_RELATIVE_DIRS
	int retval = move_tmp_to_main(tmp, main, BUDGET_DIR, BUDGET_BAK_DIR);
#else
	int retval = move_tmp_to_main(tmp, main, budget_dir, budget_bak_dir);
#endif
	return retval;
}

/* Calls move_tmp_to_main with "dir" as RECORD_DIR and "backdir" as
 * RECORD_BAK_DIR */
int mv_tmp_to_record_file(FILE *tmp, FILE* main)
{
#ifdef TB_RELATIVE_DIRS
	int retval = move_tmp_to_main(tmp, main, RECORD_DIR, RECORD_BAK_DIR);
#else
	int retval = move_tmp_to_main(tmp, main, record_dir, record_bak_dir);
#endif
	return retval;
}
