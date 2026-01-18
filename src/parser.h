#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/* Moves char * forward n fields in the CSV */
extern void seek_n_fields(char **line, int n);

/* Reads the header of the file until a newline is found */
extern int seek_beyond_header(FILE *fptr);

#endif
