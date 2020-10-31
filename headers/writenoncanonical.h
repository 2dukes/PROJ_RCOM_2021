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

/*-------------------------- Data Link Layer --------------------------*/

/*
* Aciona o Timeout da trama de controlo SET.
* Data Link Layer
*/
void setHandler(int sigNum);

/*
* Aciona o Timeout da trama de controlo DISC.
* Data Link Layer
*/
void resendDISC(int signum);

/*
* Aciona o Timeout da trama I.
* Data Link Layer
*/
void resendTrama(int sigNum);

/*
* Faz o stuffing dos bytes de informação.
* Data Link Layer
*/
int byteStuffing(unsigned char* dataToSend, int size);

/*
* Configura a porta de série, envia a trama de controlo SET e recebe a trama UA.
* Data Link Layer
*/
void llopen(struct termios* oldtio, struct termios* newtio);

/*
* Envia as tramas I.
* Data link layer
*/
void llwrite(unsigned char *buf, off_t size, bool* nTrama);

/*
* Envia a trama de controlo DISC, recebe DISC de volta e envia UA.
* Data link Layer
*/
void llclose();

/*-------------------------- Application Layer --------------------------*/

/*
* Abre um ficheiro e lê o seu conteúdo.
* Application layer
*/
unsigned char* openReadFile(char* fileName, off_t* fileSize);

/*
* Constrói os pacotes de controlo com o tamanho do ficheiro e o nome do mesmo.
* Application layer
*/
unsigned char* buildControlTrama(char* fileName, off_t* fileSize, unsigned char controlField, off_t* packageSize);

/*
* Encapsula a mensagem a enviar para ficar de acordo com os pacotes de dados.
* Application layer
*/
unsigned char* encapsulateMessage(unsigned char* messageToSend, off_t* messageSize, off_t* totalMessageSize, int* numPackets);

