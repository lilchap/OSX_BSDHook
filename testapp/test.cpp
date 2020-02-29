#include <unistd.h>
#include <iostream>

static int password = 1402;
static int currentAttempt = 0;

int login(int inputpassword) {
	if (inputpassword == password) {
		return 1;
	}
	currentAttempt++;
	return 0;
}

int main(int argc, char* argv[]) {
	int input = 0;
	printf("Program PID: %d\n", (int)getpid());
	printf("Enter in passcode: ");
	scanf("%d", &input);
	if (login(input)) {
		printf("Yeet\n");
		return 0;
	}
	printf("Login failed. Enter in passcode: ");
	scanf("%d", &input);
	if (!login(input)) {
        printf("Login failed. Program Terminating...\n");
		return 0;
	}
    printf("Yeet\n");

	return 0;
}
