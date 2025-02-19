#include "headers/generalFunctions.h"

// C Campo de Controlo
// SET (set up)                         0 0 0 0 0 0 1 1
// DISC (disconnect)                    0 0 0 0 1 0 1 1
// UA (unnumbered acknowledgment)       0 0 0 0 0 1 1 1
// RR (receiver ready / positive ACK)   R 0 0 0 0 1 0 1
// REJ (reject / negative ACK)          R 0 0 0 0 0 0 1     R = N(r)

unsigned char getCField(char typeMessage[25], bool nTrama) {
    if(strcmp(typeMessage, "SET") == 0) 
        return C_SET;
    else if(strcmp(typeMessage, "DISC") == 0)
        return C_DISC;
    else if(strcmp(typeMessage, "UA") == 0)
        return C_UA;
    else if(strcmp(typeMessage, "RR") == 0) 
        return C_RR(nTrama);
    else // if(strcmp(typeMessage, "REJ") == 0) 
        return C_REJ(nTrama);
}

unsigned char computeBcc2(unsigned char* data, int nBytes, int startPosition) {
  int result = data[startPosition];
  
  for(int i = startPosition + 1; i < startPosition + nBytes; i++)
    result ^= data[i];

  return result;
}

void configureSerialPort(int fd, struct termios* oldtio, struct termios* newtio) {

  if ( tcgetattr(fd, oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }


  bzero(newtio, sizeof(*newtio));
  newtio->c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio->c_iflag = IGNPAR;
  newtio->c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio->c_lflag = 0;

  newtio->c_cc[VTIME]    = 0;   /* inter-character timer unused */
  newtio->c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  printf("New termios structure set\n");

  if ( tcsetattr(fd,TCSANOW,newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
}

int receiveSupervisionTrama(bool withTimeout, unsigned char cField, int fd, unsigned char aField) {
  if(withTimeout)
    alarm(TIMEOUT);

  unsigned char byte;
  char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC_OK", "STOP" };
  int i = 0, res;

  // returnState:
  // 1 | readSuccessful 
  // 2 | REJ

  int returnState = 1;

  while (strcmp(state[i], "STOP") != 0) {       /* loop for input */

    res = read(fd, &byte, 1);   /* returns after 1 chars have been input */
    
    if(withTimeout) {
      if(res < 0)  // Read interrupted by a signal
        continue; // Jumps to another iteration
    }
    
    if(byte == (aField ^ cField)) { // BCC1
      if(strcmp(state[i], "C_RCV") == 0) {
          i++;
          continue;
      }
    }
    if(byte == cField || byte == C_REJ(0) || byte == C_REJ(1)) { // C
      if(strcmp(state[i], "A_RCV") == 0) {
        if(byte == C_REJ(0)) {
          returnState = 2;
          cField = C_REJ(0);
        }  
        else if(byte == C_REJ(1)) {
          returnState = 2;
          cField = C_REJ(1);
        }
        i++;
        continue;
      }
    }
    if(byte == aField) { // A
      if(strcmp(state[i], "FLAG_RCV") == 0) {
        i++;
        continue;
      }
    }

    switch (byte)
    {
      case FLAG_SET:
        if(strcmp(state[i], "START") == 0 || strcmp(state[i], "BCC_OK") == 0)
          i++;
        else
          i = 1; // STATE = FLAG_RCV
        break;      
      default: // Other_RCV
        i = 0; // STATE = START
    }
  }
    
  return returnState;
}

void sendSupervisionTrama(int fd, unsigned char cField, unsigned char aField) {
    unsigned char buf[5];
    buf[0] = FLAG_SET;
    buf[1] = aField;
    buf[2] = cField;
    buf[3] = aField ^ cField;
    buf[4] = FLAG_SET;

    write(fd, buf, sizeof(buf));   
}

void checkMaxBytesToSend(off_t* packageSize) {
  if(N_BYTES_TO_SEND < *packageSize) {
    printf("\n\nN_BYTES_TO_SEND (Data Frame size) has to be greater than the Control Frame size, which is = %ld\n\n", *packageSize);
    exit(1);
  }
  else if(N_BYTES_TO_SEND > MAX_NUM_OCTETS) {
    printf("%d\n", MAX_NUM_OCTETS);
    printf("\n\nN_BYTES_TO_SEND can\'t be larger than %d. Please choose a smaller value.\n\n", MAX_NUM_OCTETS);
    exit(1);
  }
}
