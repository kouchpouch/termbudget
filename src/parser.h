#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* Opens file located at CSV_DIR in mode. Exits on failure */
extern FILE *open_record_csv(char *mode);

/* Opens file located at TMP_DIR in w+ mode. Exits on failure */
extern FILE *open_temp_csv(void);

/* Returns the line number of byte-offset b in file CSV_DIR */
extern unsigned int boff_to_linenum(long b);

/* Moves char * forward n fields in the CSV */
extern void seek_n_fields(char **line, int n);

/* Reads the header of the file until a newline is found */
extern int seek_beyond_header(FILE *fptr);

#endif
