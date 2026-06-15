CC      := gcc
CFLAGS  := -g -Wall -Wextra -Wpedantic
LIBS    := -lncurses

SRC_DIR := src
OBJ_DIR := obj
TARGET  := termbudget

SRC_FILES := \
	main.c \
	tui.c \
	cli.c \
	tui_input.c \
	helper.c \
	sorter.c \
	parser.c \
	fileintegrity.c \
	filemanagement.c \
	tui_input_menu.c \
	tui_sidebar.c \
	get_date.c \
	convert_csv.c \
	edit_categories.c \
	edit_transaction.c \
	categories.c \
	read_init.c \
	read_loops.c \
	create.c \
	input.c \
	file_write.c \
	overview.c \
	vector.c \
	dynamic_string.c \
	vector_generic.c \

SRCS     := $(addprefix $(SRC_DIR)/, $(SRC_FILES))
OBJS     := $(SRC_FILES:%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

#termbudget:
#	gcc -g -Wall -Wextra -Wpedantic \
#		src/main.c \
#		src/cli.c \
#		src/tui.c \
#		src/tui_input.c \
#		src/helper.c \
#		src/sorter.c \
#		src/parser.c \
#		src/fileintegrity.c \
#		src/filemanagement.c \
#		src/tui_input_menu.c \
#		src/tui_sidebar.c \
#		src/get_date.c \
#		src/convert_csv.c \
#		src/edit_categories.c \
#		src/edit_transaction.c \
#		src/categories.c \
#		src/read_init.c \
#		src/read_loops.c \
#		src/create.c \
#		src/input.c \
#		src/file_write.c \
#		src/overview.c \
#		src/vector.c \
#		src/dynamic_string.c \
#		src/vector_generic.c \
#		-lncurses -o termbudget
