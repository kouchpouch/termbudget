.PHONY: termbudget

termbudget:
	gcc -g -Wall src/main.c src/tui.c src/helper.c src/sorter.c src/parser.c src/fileintegrity.c src/filemanagement.c -lncurses -o termbudget
