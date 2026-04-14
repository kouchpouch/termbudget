.PHONY: termbudget

termbudget:
	gcc -g -Wall -Wextra -Wpedantic \
		src/main.c \
		src/cli.c \
		src/tui.c \
		src/tui_input.c \
		src/helper.c \
		src/sorter.c \
		src/parser.c \
		src/fileintegrity.c \
		src/filemanagement.c \
		src/tui_input_menu.c \
		src/tui_sidebar.c \
		src/get_date.c \
		src/convert_csv.c \
		src/edit_categories.c \
		src/edit_transaction.c \
		src/categories.c \
		src/read_init.c \
		src/read_loops.c \
		src/create.c \
		src/input.c \
		src/file_write.c \
		-lncurses -o termbudget
