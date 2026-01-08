.PHONY: termbudget

termbudget:
	gcc src/main.c src/tui.c src/helper.c src/sorter.c -lncurses -o termbudget
