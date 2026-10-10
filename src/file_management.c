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
 * Copyright (c) 2026 termbudget
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
#include "file_management.h"
#include "flags.h"

enum err_data_dir {
	ERR_DATA_OK,
	ERR_DATA_NO_EXIST,
	ERR_DATA_NO_ENV,
	ERR_DATA_TOO_LONG
};

#ifndef TB_RELATIVE_DIRS

char dir_program         [PATH_BUFFER_SZ] = { 0 };
char path_record         [PATH_BUFFER_SZ] = { 0 };
char path_record_bak     [PATH_BUFFER_SZ] = { 0 };
char path_tmp_file       [PATH_BUFFER_SZ] = { 0 };
char path_converted_file [PATH_BUFFER_SZ] = { 0 };
char path_budget         [PATH_BUFFER_SZ] = { 0 };
char path_budget_bak     [PATH_BUFFER_SZ] = { 0 };
char path_config	     [PATH_BUFFER_SZ] = { 0 };

/* Prints all dir variables */
static void debug_print_file_paths(void)
{
	printf("%s\n", "------Program's files------");
	printf("%s\n", dir_program);
	printf("%s\n", path_record);
	printf("%s\n", path_record_bak);
	printf("%s\n", path_tmp_file);
	printf("%s\n", path_converted_file);
	printf("%s\n", path_budget);
	printf("%s\n", path_budget_bak);
	printf("%s\n", path_config);
}

static void set_global_path(char *dst,
							char *data_dir,
							char *file_name,
							size_t dst_sz)
{
	size_t check = 0;

	strlcat(dst, data_dir, dst_sz);
	check = strlcat(dst, file_name, dst_sz);
	if (check > dst_sz) {
		printf("\"%s\" exceeds the maximum path length\n", dst);
		exit(1);
	}
}

/* Fills the dir variables with the full path plus the file name as is defined
 * by macros. Assertions for each path to verify it does not exceed PATH_MAX */
static void set_program_paths(char *data_dir)
{
	size_t sz = PATH_BUFFER_SZ;

	set_global_path(path_record,         data_dir, RECORD_FILE,     sz);
	set_global_path(path_record_bak,     data_dir, RECORD_BAK_FILE, sz);
	set_global_path(path_tmp_file,       data_dir, TEMP_FILE,       sz);
	set_global_path(path_converted_file, data_dir, CONVERTED_FILE,  sz);
	set_global_path(path_budget,         data_dir, BUDGET_FILE,     sz);
	set_global_path(path_budget_bak,     data_dir, BUDGET_BAK_FILE, sz);

	if (debug_flag) {
		debug_print_file_paths();
	}
}

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

/* Terminates the program if unable to create the directory */
static int dir_create(char *full_path)
{
	errno = 0;
	if (mkdir(full_path, 0777) == -1) {
		if (errno == EEXIST) {
			printf("%s already exists\n", full_path);
			return 0;
		} else {
			perror("mkdir()");
			exit(1);
		}
	}

	return 0;
}

static void handle_err_data_dir(enum err_data_dir e)
{
	switch (e) {
	case ERR_DATA_OK:
		break;

	case ERR_DATA_NO_EXIST:
		printf("%s\n", "Could not find program file directory, "
		               "it does not exist");
		break;

	case ERR_DATA_NO_ENV:
		printf("%s\n", "Environment variable $HOME or $XDG_DATA_HOME "
                       "are not set");
		printf("%s\n", "Cannot find program data directory. Exiting");
		exit(1);
	
	case ERR_DATA_TOO_LONG:
		printf("%s\n", "Path exceeds maximum length");
		exit(1);

	default:
		break;
	}
}

/* Fills 'buffer' with the configuration directory using environment variables
 * XDG_DATA_HOME or HOME, whichever is set. */
static enum err_data_dir get_user_data_directory(char *buffer,
												 size_t buffer_sz)
{
	size_t cat_check = 0;
	char *dir = NULL;

	dir = getenv("XDG_DATA_HOME");
	if (dir != NULL) {
		strlcat(buffer, dir, buffer_sz);
		cat_check = strlcat(buffer, "/termbudget", buffer_sz);
		if (cat_check > buffer_sz) {
			return ERR_DATA_TOO_LONG;
		} else {
			return ERR_DATA_OK;
		}
	}

	dir = getenv("HOME");
	if (dir == NULL) {
		return ERR_DATA_NO_ENV;
	} else {
		strlcat(buffer, dir, buffer_sz);
		cat_check = strlcat(buffer, "/.local/share/termbudget", buffer_sz);
		if (cat_check > buffer_sz) {
			return ERR_DATA_TOO_LONG;
		} else {
			return ERR_DATA_OK;
		}
	}
}

/* Sets global variable path_config with the full path to the configuration
 * file. Nominally this is set to:
 * /home/$USER/.local/.share/termbudget */
static void set_user_data_path(void)
{
	char data_path_buffer[PATH_BUFFER_SZ] = { 0 };
	enum err_data_dir err = 0;

	err = get_user_data_directory(data_path_buffer, PATH_BUFFER_SZ);
	if (err != ERR_DATA_OK) {
		handle_err_data_dir(err);
	}

	if (!dir_exists(data_path_buffer)) {
		dir_create(data_path_buffer);
	}

	set_program_paths(data_path_buffer);
}

/* Fills 'buffer' with the configuration directory using environment variables
 * XDG_CONFIG_HOME or HOME, whichever is set. */
static enum err_data_dir get_configuration_directory(char *buffer,
													 size_t buffer_sz)
{
	size_t cat_check = 0;
	char *dir = NULL;

	dir = getenv("XDG_CONFIG_HOME");
	if (dir != NULL) {
		strlcat(buffer, dir, buffer_sz);
		cat_check = strlcat(buffer, "/termbudget", buffer_sz);
		if (cat_check > buffer_sz) {
			return ERR_DATA_TOO_LONG;
		} else {
			return ERR_DATA_OK;
		}
	}

	dir = getenv("HOME");
	if (dir == NULL) {
		return ERR_DATA_NO_ENV;
	} else {
		strlcat(buffer, dir, buffer_sz);
		cat_check = strlcat(buffer, "/.config/termbudget", buffer_sz);
		if (cat_check > buffer_sz) {
			return ERR_DATA_TOO_LONG;
		} else {
			return ERR_DATA_OK;
		}
	}
}

/* Sets global variable path_config with the full path to the configuration
 * file. Nominally this is set to:
 * /home/$USER/.config/termbudget/termbudget.conf */
int set_configuration_path(void)
{
	char config_path_buffer[PATH_BUFFER_SZ] = { 0 };
	enum err_data_dir err = 0;

	err = get_configuration_directory(config_path_buffer, PATH_BUFFER_SZ);
	if (err != ERR_DATA_OK) {
		handle_err_data_dir(err);
	}

	if (!dir_exists(config_path_buffer)) {
		dir_create(config_path_buffer);
	}

	if (strlcat(config_path_buffer, CONFIG_FILE, PATH_BUFFER_SZ) > PATH_BUFFER_SZ) {
		printf("%s\n", "Path too long");
		exit(1);
	}
	strlcpy(path_config, config_path_buffer, PATH_BUFFER_SZ);

	return 0;
}

/* Upon reaching a permission error, no env error, or a mkdir() error, or
 * if the path exceeds MAX_PATH, the error is printed and the program will 
 * terminate. */
void set_termbudget_file_paths(void)
{
	set_configuration_path();
	set_user_data_path();
}

#endif

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
