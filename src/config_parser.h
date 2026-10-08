#ifndef COLOR_PARSER_H
#define COLOR_PARSER_H

#include <stdio.h>

#define CONFIG_BUFFER_SZ 256
#define COMMENT_PREFIX '#'

enum slice_get_which {
	KEY,
	VALUE
};

enum parse_error {
	PARSE_EOF,
	PARSE_BLANK,
	PARSE_OK
};

enum keys_config {
	KEY_UNKNOWN = -1,
	KEY_COLOR0,
	KEY_COLOR1,
	KEY_COLOR2,
	KEY_COLOR3,
	KEY_COLOR4,
	KEY_COLOR5,
	KEY_COLOR6,
	KEY_COLOR7,
	KEY_COLOR8,
	KEY_COLOR9
};

enum keys_type {
	KEY_TYPE_UNKNOWN,
	KEY_TYPE_BOOLEAN,
	KEY_TYPE_STRING,
	KEY_TYPE_INTEGER,
	KEY_TYPE_COLOR,
};

struct string_slice {
	char *s;
	int len;
};

struct key_type_pair {
	char *key;
	enum keys_type t;
};

struct key_value_pair {
	struct string_slice key;
	struct string_slice value;
};

void slice_print(const struct string_slice slice);

enum keys_config parse_config_enumerate_key(const struct string_slice key);

enum keys_type parse_config_enumerate_key_type(const struct string_slice key);

enum parse_error parse_config(FILE *conf,
							  struct string_slice *key,
							  struct string_slice *value,
							  char *buff,
							  size_t buff_sz);

#endif
