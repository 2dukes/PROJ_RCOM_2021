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
#include "generalFunctions.h"

int receiveTrama(int* nTrama, int fd, unsigned char* receivedMessage);

void llread(int fd, int numPackets);