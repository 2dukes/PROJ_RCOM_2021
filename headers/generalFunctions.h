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

/*
* Retorna o Campo de Controlo (C) especificado. 
*/
unsigned char getCField(char typeMessage[25], bool nTrama);

/*
* Faz o parse do BCC2 de acordo com o pacote de dados recebido.
*/
unsigned char computeBcc2(unsigned char data[], int nBytes, int startPosition);

/*
*     Open serial port device for reading and writing and not as controlling tty
*      because we don't want to get killed if linenoise sends CTRL-C.
*/
void configureSerialPort(int fd, struct termios* oldtio, struct termios* newtio);

/*
* Lê uma trama de Supervisão.
*/
int receiveSupervisionTrama(bool withTimeout, unsigned char cField, int fd);

/*
* Envia uma trama de Supervisão.
*/
void sendSupervisionTrama(int fd, unsigned char cField);

/*
* Verifica se o número máximo de Bytes a enviar foi excedido (2^16)
*/
void checkMaxBytesToSend(off_t* packageSize);

struct readReturn {
    unsigned char* currentMessage;
    int currentMessageSize;
};

struct receiveTramaReturn {
    unsigned char* currentMessage;
    int statusCode;
    int currentMessageSize;
};