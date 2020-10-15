#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "macros.h" 

unsigned char getCField(char typeMessage[25], bool nTrama);

unsigned char computeBcc2(unsigned char data[BUF_MAX_SIZE], int nBytes, int startPosition);

/*
      Open serial port device for reading and writing and not as controlling tty
      because we don't want to get killed if linenoise sends CTRL-C.
*/
int configureSerialPort(char argv1[50], struct termios* oldtio, struct termios* newtio);

bool receiveSupervisionTrama(bool withTimeout, unsigned char cField, int fd);

void sendSupervisionTrama(int fd, unsigned char cField);