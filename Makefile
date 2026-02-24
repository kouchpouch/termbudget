.PHONY: termbudget

termbudget:
	gcc -g -Wall \
		src/main.c \
		src/tui.c \
		src/tui_input.c \
		src/helper.c \
		src/sorter.c \
		src/parser.c \
		src/fileintegrity.c \
		src/filemanagement.c \
		src/tui_input_menu.c \
		-lncurses -o termbudget
