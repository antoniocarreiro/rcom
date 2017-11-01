#include <stdio.h>
#include <string.h>
#include "Application.h"

void printUsage(char *command) {
	printf("Usage: %s <mode> <port> <file>\n", command);
	printf("Where <mode> includes:\n");
	printf("    -s    Send a file\n");
	printf("    -r    Receive a file\n");
	printf("And <port> includes:\n");
	printf("    /dev/ttyS0\n");
	printf("    /dev/ttyS1\n");
}

int processArguments(char **argv) {
	int mode;
	char *port, *fileName;

	if (strcmp(argv[1], "-s") == 0)
		mode = SEND;
	else if (strcmp(argv[1], "-r") == 0)
		mode = RECEIVE;
	else {
		printf("ERROR: Neither send or receive was specified.\n");
		printUsage(argv[0]);
		return -1;
	}

	if (strcmp(argv[2], "/dev/ttyS0") == 0 || strcmp(argv[2], "/dev/ttyS1") == 0)
		port = argv[2];
	else {
		printf("ERROR: Please choose a valid port.\n");
		printUsage(argv[0]);
		return -1;
	}

	if (mode == SEND) {
		FILE *file;
		file = fopen(argv[3], "r");

		if (file == NULL) {
			printf("ERROR: File \"%s\" does not exist.\n", argv[3]);
			printUsage(argv[0]);
			return -1;
		} else {
			fclose(file);
			fileName = argv[3];
		}
	} else if (mode == RECEIVE) {
		fileName = argv[3];
	}

	initApplication(mode, port, fileName);

	return 0;
}

int main(int argc, char **argv) {
	if (argc != 4) {
		printf("ERROR: Wrong number of arguments.\n");
		printUsage(argv[0]);
		return -1;
	} else {
		processArguments(argv);
	}

	return 0;
}
