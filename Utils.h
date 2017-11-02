#ifndef UTILS_H
#define UTILS_H

#define TRUE             1
#define FALSE            0

#define SEND             0
#define RECEIVE          1

#define UA               0
#define SET              1
#define DISC             2
#define RR               3
#define REJ              4

#define A                0x03
#define FLAG             0x7E
#define DATA             0x01
#define ESCAPE           0x7D

#define C_UA             0x03
#define C_SET            0x07
#define C_DISC           0x0B
#define C_RR             0x05
#define C_REJ            0x01

#define START            0
#define FLAG_RCV         1
#define A_RCV            2
#define C_RCV            3
#define BCC_OK           4
#define DONE             5

#define COMMAND_SIZE     5
#define DATA_PKG_SIZE    4
#define CTRL_PKG_SIZE    11

#define CTRL_PKG_DATA    1
#define CTRL_PKG_START   2
#define CTRL_PKG_END     3

#define PARAM_FILE_SIZE  0
#define PARAM_FILE_NAME  1

#define _POSIX_SOURCE    1
#define PROGRESS_BAR     45
#define MAX_SIZE         512
#define MAX 131085
#define BAUDRATE         B38400

#include <stdio.h>
#include <termios.h>
#include <sys/stat.h>

typedef struct {
	int fd, mode;
	char *port, *fileName;
} Application;

typedef struct {
	int ns, retries, timeout;
	struct termios oldtio, newtio;
} DataLink;

typedef struct {
	int numberTimeout;
	int rejSent, rejReceived;
	int frameSent, frameResent, frameReceived;
} Statistics;

DataLink *dl;
Application *app;
Statistics *stats;

volatile int FRAME_SIZE, STUFFED_SIZE;

long int getFileSize(char *fileName) {
	struct stat st;

	if (stat(fileName, &st) == 0)
		return st.st_size;

	printf("ERROR: Could not get file size.\n");
	return -1;
}

void printBuffer(unsigned char *buf, int size) {
	int i;
	for (i = 0; i < size; i++)
		printf("0x%02x ", buf[i]);
}

void printProgress(float curr, float total) {
	float per = (100.0 * curr) / total;
	printf("Completed: %6.2f%% [", per);

	int i, pos = (per * PROGRESS_BAR) / 100.0;
	for (i = 0; i < PROGRESS_BAR; i++) {
		if (i <= pos)
			printf("#");
		else
			printf(" ");
	}

	printf("]\r");
	fflush(stdout);
}

void printStatistics() {
	printf("\n - Statistics - \n");
	printf(" # Number of timeouts: %d\n", stats->numberTimeout);
	printf(" # Number of REJs sent: %d\n", stats->rejSent);
	printf(" # Number of REJs received: %d\n", stats->rejReceived);
	printf(" # Number of Frames sent: %d\n", stats->frameSent);
	printf(" # Number of Frames resent: %d\n", stats->frameResent);
	printf(" # Number of Frames received: %d\n", stats->frameReceived);
}

#endif
