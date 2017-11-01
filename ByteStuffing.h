#ifndef BYTESTUFFING_H
#define BYTESTUFFING_H

#include <stdlib.h>
#include <string.h>
#include "Utils.h"

unsigned char *stuff(unsigned char *buf, int length, int *newSize) {
	int i, j = 0;
	unsigned char *stuffed = (unsigned char *)malloc(length * 2);

	for(i = 0; i < length; i++) {
		if(buf[i] == FLAG && i != 0 && i != (length - 1)) {
			stuffed[j] = ESCAPE;
			stuffed[j + 1] = FLAG ^ 0x20;
			j = j + 2;
		} else if(buf[i] == ESCAPE) {
			stuffed[j] = ESCAPE;
			stuffed[j + 1] = ESCAPE ^ 0x20;
			j = j + 2;
		} else {
			stuffed[j] = buf[i];
			j++;
		}
	}

	*newSize = j;
	return stuffed;
}

unsigned char *destuff(unsigned char *buf, int length) {
	int i, j = 0;
	unsigned char *destuffed = (unsigned char *)malloc(length);

	for(i = 0; i < length; i++) {
		if(buf[i] == ESCAPE && buf[i + 1] == (FLAG ^ 0x20)) {
			destuffed[j] = FLAG;
			j++;
			i++;
		} else if(buf[i] == ESCAPE && buf[i + 1] == (ESCAPE ^ 0x20)) {
			destuffed[j] = ESCAPE;
			j++;
			i++;
		} else {
			destuffed[j] = buf[i];
			j++;
		}
	}

	return destuffed;
}

#endif
