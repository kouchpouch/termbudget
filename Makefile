.PHONY: termbudget

termbudget:
	gcc -Wall src/main.c src/tui.c src/helper.c src/sorter.c -lncurses -o termbudget
