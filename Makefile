.PHONY: termbudget

termbudget:
	gcc main.c tui.c helper.c sorter.c -lncurses -o termbudget
