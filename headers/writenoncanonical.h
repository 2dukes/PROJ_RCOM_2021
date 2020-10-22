#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "macros.h"
#include "generalFunctions.h"

void alarmHandler(int sigNum);

void resendTrama(int sigNum);

void llopen(struct termios* oldtio, struct termios* newtio);

void llwrite(unsigned char* buf, off_t size);