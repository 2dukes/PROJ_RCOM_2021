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
#include <math.h>

#include "macros.h"
#include "generalFunctions.h"

/*-------------------------- Data Link Layer --------------------------*/

/*
* Faz o destuffing de uma mensagem.
* Data Link Layer
*/
struct readReturn deStuffing(unsigned char * message, int size);

/*
* Configura a porta de série, lê a trama de controlo SET e envia a trama UA.
* Data Link Layer
*/
void llopen(int fd, struct termios* oldtio, struct termios* newtio);

/*
* Lê tramas I.
* Data Link Layer
*/
struct readReturn llread(int fd, int* tNumber);

/*
* Recebe os diferentes bytes da trama.
* Data Link Layer
*/
struct receiveTramaReturn receiveTrama(int* nTrama, int fd);

/*
* Lê trama de controlo DISC, envia DISC de volta e recebe UA.
* Data link Layer
*/
void llclose(int fd);

/*-------------------------- Application Layer --------------------------*/

/*
* Verifica se é a última mensagem.
* Application Layer
*/
bool checkEnd(unsigned char* endMessage, off_t endMessageSize, unsigned char* startMessage, off_t startMessageSize);

/*
* Obtém o tamanho do ficheiro a partir da trama START.
* Application Layer
*/
off_t sizeOfFile_Start(unsigned char *start);

/*
* Obtém o nome do ficheiro a partir da trama START.
* Application Layer
*/
unsigned char *nameOfFile_Start(unsigned char *start);

/*
* Cria ficheiro com os dados recebidos nas tramas I.
* Application Layer
*/
void createFile(unsigned char *data, off_t* sizeFile, unsigned char filename[]);
