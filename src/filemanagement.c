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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "dynamic_string.h"
#include "filemanagement.h"
#include "flags.h"

#ifndef TB_RELATIVE_DIRS

char dir_program         [PATH_MAX];
char path_record         [PATH_MAX];
char path_record_bak     [PATH_MAX];
char path_tmp_file       [PATH_MAX];
char path_converted_file [PATH_MAX];
char path_budget         [PATH_MAX];
char path_budget_bak     [PATH_MAX];
char path_config	     [PATH_MAX];

/* Sets all dir variables to zero */
static void init_dir_variables(void)
{
	memset(dir_program,         0, sizeof(dir_program));
	memset(path_record,         0, sizeof(path_record));
	memset(path_record_bak,     0, sizeof(path_record_bak));
	memset(path_tmp_file,       0, sizeof(path_tmp_file));
	memset(path_converted_file, 0, sizeof(path_converted_file));
	memset(path_budget,         0, sizeof(path_budget));
	memset(path_budget_bak,     0, sizeof(path_budget_bak));
	memset(path_config,     	0, sizeof(path_config));
}

/* Fills the dir variables with the full path plus the file name as is defined
 * by macros. Assertions for each path to verify it does not exceed PATH_MAX */
static void set_directories(struct d_string *p)
{
	strcat(dir_program, p->string);

	strcat(path_record, dir_program);
	assert(strlen(path_record) + strlen(RECORD_FILE) < PATH_MAX);
	strcat(path_record, RECORD_FILE);

	strcat(path_record_bak, dir_program);
	assert(strlen(path_record_bak) + strlen(RECORD_BAK_FILE) < PATH_MAX);
	strcat(path_record_bak, RECORD_BAK_FILE);

	strcat(path_tmp_file, dir_program);
	assert(strlen(path_tmp_file) + strlen(TEMP_FILE) < PATH_MAX);
	strcat(path_tmp_file, TEMP_FILE);

	strcat(path_converted_file, dir_program);
	assert(strlen(path_converted_file) + strlen(CONVERTED_FILE) < PATH_MAX);
	strcat(path_converted_file, CONVERTED_FILE);

	strcat(path_budget, dir_program);
	assert(strlen(path_budget) + strlen(BUDGET_FILE) < PATH_MAX);
	strcat(path_budget, BUDGET_FILE);

	strcat(path_budget_bak, dir_program);
	assert(strlen(path_budget_bak) + strlen(BUDGET_BAK_FILE) < PATH_MAX);
	strcat(path_budget_bak, BUDGET_BAK_FILE);
}

/* Prints all dir variables */
static void debug_print_directories(void)
{
	printf("%s\n", "------Program files------");
	printf("%s\n", dir_program);
	printf("%s\n", path_record);
	printf("%s\n", path_record_bak);
	printf("%s\n", path_tmp_file);
	printf("%s\n", path_converted_file);
	printf("%s\n", path_budget);
	printf("%s\n", path_budget_bak);
	printf("%s\n", path_config);
}

#endif

enum err_data_dir {
	ERR_DATA_OK = 0,
	ERR_DATA_NOEXIST,
	ERR_DATA_NOENV,
};

/* Returns true if a the dir at 'path' exists, false if it does not.
 * If opendir() fails, errno is printed to stderror */
static bool dir_exists(char *path)
{
	errno = 0;
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

/* Returns ERR_DATA_OK on success and concatenates the 'path' with the
 * directory returned by 'env' */
static enum err_data_dir dir_get_by_env(struct d_string *path, char *env)
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

/* Concatenates the full path to the user data directory on 'path_buffer' */
static enum err_data_dir dir_get_user_data(struct d_string *path_buffer)
{
	enum err_data_dir e;
	char *append_dir = "/.local/share";

	if ((e = dir_get_by_env(path_buffer, "XDG_DATA_HOME")) != ERR_DATA_OK) {
		if ((e = dir_get_by_env(path_buffer, "HOME")) != ERR_DATA_OK) {
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

static enum err_data_dir dir_get_user_config(struct d_string *path_buffer)
{
	enum err_data_dir e;
	char *append_dir = "/.config";

	if ((e = dir_get_by_env(path_buffer, "XDG_CONFIG_HOME")) != ERR_DATA_OK) {
		if ((e = dir_get_by_env(path_buffer, "HOME")) != ERR_DATA_OK) {
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

static int dir_create(char *full_path)
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
		printf("%s\n", "Could not find program file directory, "
		               "it does not exist");
		break;

	case ERR_DATA_NOENV:
		printf("%s\n", "Environment variable $HOME or $XDG_DATA_HOME "
                        "are not set");
		printf("%s\n", "Cannot find program data directory. Exiting");
		exit(1);
	default:
		break;
	}
}

int dir_program_create(void)
{
	struct d_string *full_path = create_d_string(PATH_MAX);
	char *subdir_name = "/termbudget";

	enum err_data_dir err = 0;
	err = dir_get_user_data(full_path);
	if (err != ERR_DATA_OK) {
		handle_err_data_dir(err);
	}

	concatenate_d_string(&full_path, subdir_name, strlen(subdir_name));

	if (!dir_exists(full_path->string)) {
		dir_create(full_path->string);
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

int dir_config_create(void)
{
	struct d_string *full_path = create_d_string(PATH_MAX);
	char *subdir_name = "/termbudget";

	enum err_data_dir err = 0;
	err = dir_get_user_config(full_path);
	if (err != ERR_DATA_OK) {
		handle_err_data_dir(err);
	}

	concatenate_d_string(&full_path, subdir_name, strlen(subdir_name));

	if (!dir_exists(full_path->string)) {
		dir_create(full_path->string);
	} else {
		if (debug_flag) {
			printf("%s exists\n", full_path->string);
		}
	}

	if (debug_flag) {
		printf("%s\n", full_path->string);
	}

	assert(full_path->len < PATH_MAX);

#ifndef TB_RELATIVE_DIRS
	strcat(path_config, full_path->string);
	assert(strlen(path_config) + strlen(CONFIG_FILE) < PATH_MAX);
	strcat(path_config, CONFIG_FILE); 
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

/* Unified function to create all of the program files. 
 * Returns -1 on failure, 0 on success. */
int create_program_files(void)
{
	int ret = 0;


	return ret;
}

/* Opens file at "dir", checks if the fopen function fails and terminates
 * program if it does. */
static FILE *open_file(char *mode, char *path)
{
	FILE *fptr = fopen(path, mode);
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
	FILE *f = open_file(mode, path_budget);
#endif
	return f;
}

/* Opens RECORD_DIR with open_file() */
FILE *open_record_csv(char *mode)
{
#ifdef TB_RELATIVE_DIRS
	FILE *f = open_file(mode, RECORD_DIR);
#else
	FILE *f = open_file(mode, path_record);
#endif
	return f;
}

FILE *open_config_file(char *mode)
{
#ifdef TB_RELATIVE_DIRS
	FILE *f = open_file(mode, CONFIG_DIR);
#else
	FILE *f = open_file(mode, path_config);
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
	FILE *tmpfptr = fopen(path_tmp_file, "w+");
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
	if (rename(path_tmp_file, dir) == -1) {
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
	int retval = move_tmp_to_main(tmp, main, path_budget, path_budget_bak);
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
	int retval = move_tmp_to_main(tmp, main, path_record, path_record_bak);
#endif
	return retval;
}
