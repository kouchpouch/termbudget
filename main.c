#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

int addExpense(int balance, int expense) {
	balance = balance - expense;
	return balance;
}

int main() {
	int balance = 1000;
	int expense = 0;
	int newBalance;
	while (balance >= 0) {
		printf("Enter an expense\n");
		scanf("%d", &expense);
		newBalance = addExpense(balance, expense);
		balance = newBalance;
		if (newBalance < 0) {
			printf("Balance is negative! %d\n", newBalance);
		} else {
			printf("Balance is %d\n", newBalance);
		}
	}
	return 0;
}
