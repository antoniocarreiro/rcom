#ifndef DATALINK_H
#define DATALINK_H

#include <stdio.h>
#include <stdlib.h>
#include "Utils.h"
#include "Alarm.h"
#include "ByteStuffing.h"

volatile int STOP = FALSE;

int initDataLink() {
	dl = (DataLink *)malloc(sizeof(DataLink));

	dl->ns = 0;
	dl->retries = 3;
	dl->timeout = 3;

	// Save current port settings
	if (tcgetattr(app->fd, &dl->oldtio) == -1) {
		printf("ERROR: Could not save current port settings.\n");
		return -1;
	}

	// Set new termios structure
	bzero(&dl->newtio, sizeof(dl->newtio));
	dl->newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	dl->newtio.c_iflag = IGNPAR;
	dl->newtio.c_oflag = 0;
	dl->newtio.c_lflag = 0;
	dl->newtio.c_cc[VMIN] = 1;
	dl->newtio.c_cc[VTIME] = 0;

	tcflush(app->fd, TCIOFLUSH);
	if (tcsetattr(app->fd, TCSANOW, &dl->newtio) == -1) {
		printf("ERROR: Failed to set new termios structure.\n");
		return -1;
	}

	return 0;
}

unsigned char *createCommand(int command) {
	unsigned char *buf = (unsigned char *)malloc(COMMAND_SIZE);

	buf[0] = FLAG;
	buf[1] = A;

	switch (command) {
	case UA:
		buf[2] = C_UA;
		break;
	case SET:
		buf[2] = C_SET;
		break;
	case DISC:
		buf[2] = C_DISC;
		break;
	case RR:
		buf[2] = C_RR;
		break;
	case REJ:
		buf[2] = C_REJ;
		break;
	}

	buf[3] = buf[1] ^ buf[2];
	buf[4] = FLAG;

	return buf;
}

unsigned char *createFrame(unsigned char *buffer, int length) {
	int i;
	unsigned char BCC2 = 0x00;

	FRAME_SIZE = length + 6;
	unsigned char *frame = (unsigned char *)malloc(length + 6);

	frame[0] = FLAG;
	frame[1] = A;
	frame[2] = dl->ns << 6;
	frame[3] = frame[1] ^ frame[2];

	for (i = 0; i < length; i++) {
		BCC2 ^= buffer[i];
		frame[i + 4] = buffer[i];
	}

	frame[length + 4] = BCC2;
	frame[length + 5] = FLAG;

	return frame;
}

int llopen(int fd, int mode) {
	int res = 0;
	unsigned char x, flag;
	unsigned char answer[COMMAND_SIZE];

	switch (mode) {
	case SEND:
		COMMAND = createCommand(SET);

		(void)signal(SIGALRM, connect);
		alarm(dl->timeout);

		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send SET buffer.\n");
			return -1;
		}

		while (STOP == FALSE) {
			res += read(fd, &x, 1);

			if (res == 1 && x == FLAG)
				flag = x;
			else if (res == 1 && x != FLAG)
				res = 0;
			if (x == flag && res > 1)
				STOP = TRUE;

			answer[res - 1] = x;
		}

		if (answer[3] != (A ^ C_UA)) {
			printf("ERROR: Failure on initial connection.\n");
			return -1;
		}

		alarm(0);
		break;
	case RECEIVE:
		while (STOP == FALSE) {
			res += read(fd, &x, 1);

			if (res == 1 && x == FLAG)
				flag = x;
			else if (res == 1 && x != FLAG)
				res = 0;
			if (x == flag && res > 1)
				STOP = TRUE;

			answer[res - 1] = x;
		}

		if (answer[3] != (A ^ C_SET)) {
			printf("ERROR: Failure on initial connection.\n");
			return -1;
		}

		COMMAND = createCommand(UA);
		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send UA buffer.\n");
			return -1;
		}
		break;
	}

	return 0;
}

int llwrite(int fd, unsigned char *buffer, int length) {
	int res = 0, newSize = 0;
	unsigned char x, flag;
	unsigned char answer[COMMAND_SIZE];

	unsigned char *frame = createFrame(buffer, length);
	STUFFED = stuff(frame, FRAME_SIZE, &newSize);
	STUFFED_SIZE = newSize;

	(void)signal(SIGALRM, send);
	alarm(dl->timeout);

	write(fd, STUFFED, STUFFED_SIZE);

	STOP = FALSE;
	while (STOP == FALSE) {
		res += read(fd, &x, 1);

		if (res == 1 && x == FLAG)
			flag = x;
		else if (res == 1 && x != FLAG)
			res = 0;
		if (x == flag && res > 1)
			STOP = TRUE;

		answer[res - 1] = x;
	}

	if (answer[3] == (A ^ C_RR)) {
		alarm(0);
		triesSend = 0;
		free(STUFFED);
		return STUFFED_SIZE;
	} else if (answer[3] == (A ^ C_REJ)) {
		alarm(0);
		triesSend = 0;
		stats->rejReceived++;
		return -2;
	}

}

int llread(int fd, unsigned char **buffer) {
	int size = 0;
	volatile int over = FALSE, state = START;
	unsigned char *fileBuf = (unsigned char *)malloc((MAX_SIZE + 10) * 2);

	while (over == FALSE) {
		unsigned char x;

		if (state != DONE) {
			if (read(app->fd, &x, 1) == -1)
				return -1;
		}

		switch (state) {
		case START:
			if (x == FLAG) {
				fileBuf[size++] = x;
				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (x == A) {
				fileBuf[size++] = x;
				state = A_RCV;
			} else if (x != FLAG) {
				size = 0;
				state = START;
			}
			break;
		case A_RCV:
			if (x != FLAG) {
				fileBuf[size++] = x;
				state = C_RCV;
			} else if (x == FLAG) {
				size = 1;
				state = FLAG_RCV;
			} else {
				size = 0;
				state = START;
			}
			break;
		case C_RCV:
			if (x == (fileBuf[1] ^ fileBuf[2])) {
				fileBuf[size++] = x;
				state = BCC_OK;
			} else if (x == FLAG) {
				size = 1;
				state = FLAG_RCV;
			} else {
				size = 0;
				state = START;
			}
			break;
		case BCC_OK:
			if (x == FLAG) {
				fileBuf[size++] = x;
				state = DONE;
			} else if (x != FLAG) {
				if (size > 65540) {
					COMMAND = createCommand(REJ);
					stats->rejSent++;

					if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
						printf("ERROR: Failed to send REJ buffer.\n");
						return -1;
					}

					return -2;
				}
				
				fileBuf[size++] = x;
			}
			break;
		case DONE:
			fileBuf[size] = 0;
			over = TRUE;
			break;
		}
	}

	int k;
	unsigned char *destuffed = destuff(fileBuf, size);
	unsigned char BCC2 = 0x00;

	if (destuffed[3] != (destuffed[1] ^ destuffed[2])) {
		COMMAND = createCommand(REJ);
		stats->rejSent++;

		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send REJ buffer.\n");
			return -1;
		}

		return -2;
	} else if (destuffed[2] != C_SET && destuffed[2] != C_UA && destuffed[2] != C_DISC && destuffed[2] != C_RR && destuffed[2] != C_REJ) {
		int i;

		if (destuffed[4] == CTRL_PKG_START || destuffed[4] == CTRL_PKG_END)
			k = destuffed[6] + 3;
		else if (destuffed[4] == CTRL_PKG_DATA)
			k = (destuffed[6] * 256) + destuffed[7] + 4;

		for (i = 0; i < k; i++)
			BCC2 ^= destuffed[i + 4];

		if (BCC2 != destuffed[4 + k]) {
			COMMAND = createCommand(REJ);
			stats->rejSent++;

			if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
				printf("ERROR: Failed to send REJ buffer.\n");
				return -1;
			}

			return -2;
		} else {
			COMMAND = createCommand(RR);

			if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
				printf("ERROR: Failed to send RR buffer.\n");
				return -1;
			}
		}
	}

	free(fileBuf);
	*buffer = destuffed;

	return k - 4;
}

int llclose(int fd, int mode) {
	int res = 0;
	unsigned char x, flag;
	unsigned char answer[COMMAND_SIZE];

	switch (mode) {
	case SEND:
		COMMAND = createCommand(DISC);
		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send DISC buffer.\n");
			return -1;
		}

		STOP = FALSE;
		while (STOP == FALSE) {
			res += read(fd, &x, 1);

			if (res == 1 && x == FLAG)
				flag = x;
			else if (res == 1 && x != FLAG)
				res = 0;
			if (x == flag && res > 1)
				STOP = TRUE;

			answer[res - 1] = x;
		}

		if (answer[3] != (A ^ C_DISC)) {
			printf("ERROR: Failure on closing connection.\n");
			return -1;
		}

		COMMAND = createCommand(UA);
		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send UA buffer.\n");
			return -1;
		}
		break;
	case RECEIVE:
		STOP = FALSE;
		while (STOP == FALSE) {
			res += read(fd, &x, 1);

			if (res == 1 && x == FLAG)
				flag = x;
			else if (res == 1 && x != FLAG)
				res = 0;
			if (x == flag && res > 1)
				STOP = TRUE;

			answer[res - 1] = x;
		}

		if (answer[3] != (A ^ C_DISC)) {
			printf("ERROR: Failure on closing connection.\n");
			return -1;
		}

		COMMAND = createCommand(DISC);
		if (write(fd, COMMAND, COMMAND_SIZE) == -1) {
			printf("ERROR: Failed to send DISC buffer.\n");
			return -1;
		}

		res = 0;
		STOP = FALSE;
		while (STOP == FALSE) {
			res += read(fd, &x, 1);

			if (res == 1 && x == FLAG)
				flag = x;
			else if (res == 1 && x != FLAG)
				res = 0;
			if (x == flag && res > 1)
				STOP = TRUE;

			answer[res - 1] = x;
		}

		if (answer[3] != (A ^ C_UA)) {
			printf("ERROR: Failure on closing connection.\n");
			return -1;
		}
		break;
	}

	return 0;
}

int closeSerialPort() {
	if (tcsetattr(app->fd, TCSANOW, &dl->oldtio) == -1) {
		perror("tcsetattr");
		return -1;
	}

	close(app->fd);
	return 0;
}

#endif
