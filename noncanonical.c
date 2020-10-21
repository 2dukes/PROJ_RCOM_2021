/*Non-Canonical Input Processing*/

#include "headers/noncanonical.h"

volatile int STOP=FALSE;

int deStuffing(unsigned char * message, int size) {
  int currentMessageSize = 0;
  unsigned char* dataBytes = (unsigned char *) malloc(0);

  for(int i = 0; i < size; i++) {
    dataBytes = (unsigned char*) realloc(dataBytes, currentMessageSize + 1);

    // printf("message[%d] = %p\n", i, message[i]);
    if(message[i] == ESC) {
      if(i + 1 < size) {
        if(message[i+1] == ESC_XOR1) {
          dataBytes[currentMessageSize++] = FLAG_SET;
          i++;
        } else if(message[i+1] == ESC_XOR2) {
          dataBytes[currentMessageSize++] = ESC;
          i++;
        }
      } else {
        printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
        return -1;
      }
    } else if(message[i] == FLAG_SET){
      printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
      return -1;
    } else
      dataBytes[currentMessageSize++] = message[i];
    
    // printf("dataBytes[%d] = %p\n", currentMessageSize - 1, dataBytes[currentMessageSize - 1]);
  }

  memset(message, 0, size); // Clear array
  memcpy(message, dataBytes, currentMessageSize); // Substitute original array with destuffed content

  free(dataBytes);
  
  return currentMessageSize;
}

int receiveTrama(int* nTrama, int fd, unsigned char* receivedMessage) {
  unsigned char buf;
  char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC1_OK", "STOP" };
  int i = 0, res;
  bool repeatedByte = false;

  bool cFlag; // !nTrama = [Ns = 0 | 1]

  unsigned char dataBytes[BUF_MAX_SIZE];
  int index = 0;

  while (strcmp(state[i], "STOP") != 0) {       /* loop for input */
    printf("\nSTATE (R): %s\n", state[i]);
    res = read(fd, &buf, 1);   /* returns after 1 chars have been input */
    printf("%p\n", buf);

    // Special cases (as they use dynamic fields)
    if(buf == (A_C_SET ^ C_I(*nTrama)) || buf == (A_C_SET ^ C_I(!(*nTrama)))) { // BCC1
      if(strcmp(state[i], "C_RCV") == 0) {
        i++;
        continue;
      }
    }
    else if(buf == C_I(0)) { // C -> Determined in run-time
      if(strcmp(state[i], "A_RCV") == 0) {
        cFlag = 0; 
        i++;
        continue;
      }
    }
    else if(buf == C_I(1)) { // C -> Determined in run-time
      if(strcmp(state[i], "A_RCV") == 0) {
        cFlag = 1;
        i++;
        continue;
      }
    }

    switch (buf)
    {
      case FLAG_SET:
        if(strcmp(state[i], "START") == 0 || strcmp(state[i], "BCC1_OK") == 0)
          i++;
        else if(strcmp(state[i], "BCC1_OK") != 0)
          i = 1; // STATE = FLAG_RCV
        break;
      case A_C_SET: // A 
        if(strcmp(state[i], "FLAG_RCV") == 0)
          i++;         
        else if(strcmp(state[i], "BCC1_OK") != 0)
          i = 0; // Other_RCV
        else
          dataBytes[index++] = buf;
        break;  

      default: 
        if(strcmp(state[i], "BCC1_OK") == 0) 
          dataBytes[index++] = buf;
        else // Other_RCV
          i = 0;
    }
  } 

  if(cFlag == *nTrama) // repeatedByte
    repeatedByte = true;


  // Destuffing - Including BCC2
  int currentMessageSize = deStuffing(dataBytes, index);
  if(currentMessageSize == -1)
    return -1; // PEDIR RETRANSMISSÃO (REJ)
  
  // At this point dataBytes[index - 1] holds BCC2
  unsigned char bcc2 = computeBcc2(dataBytes, currentMessageSize - 1, 0); // Excluding BCC2
  if(bcc2 != dataBytes[currentMessageSize - 1]) {
    if(repeatedByte) {
      *nTrama = cFlag;
      return 2; // Status Code for Repeated Byte -> Descartar campo de dados
    }
    else 
      return -1; // Status Code Error -> PEDIR RETRANSMISSÃO (REJ)
  }
  *nTrama = cFlag;
  if(repeatedByte) 
    return 2; // Status Code for Repeated Byte -> Descartar campo de dados
  return 0;
}

void llopen(int fd, struct termios* oldtio, struct termios* newtio) {
  configureSerialPort(fd, oldtio, newtio);

  // RECEIVE SET
  receiveSupervisionTrama(false, getCField("SET", false), fd);
  printf("\n--- RECEIVING SET ---\n");

  // SEND UA 
  sendSupervisionTrama(fd, getCField("UA", false));
  printf("\n--- SENDING UA ---\n");

}

void llread(int fd, int numPackets) {
  int i = 0, statusCode;
  int tNumber = -1; // [Nr = 0 | 1]

  unsigned char messageRead[N_BYTES_TO_SEND * 2]; // Temporary Buffer

  while(i < numPackets) {
    // VALORES DE C são gerados consoante recebidos! CORRIGIR
    statusCode = receiveTrama(&tNumber, fd, messageRead);
    if(statusCode == 0) {
      printf("\nReceived Trama %d with success!\n \nSendign RR (%d)\n", i, !tNumber);
      sendSupervisionTrama(fd, getCField("RR", !tNumber));
      i++;
    }
    else {
      printf("Didn't receive Trama %d with success!\n", i);
      if(statusCode == 2) {
        printf("\n-- Repeated Byte --\n\nSendign RR (%d)\n", !tNumber);
        sendSupervisionTrama(fd, getCField("RR",!tNumber));
      }
      else if(statusCode == -1) { // Send REJ
        printf("\n-- Retransmit Byte --\n\nSendign REJ (%d)\n", tNumber);
        sendSupervisionTrama(fd, getCField("REJ", tNumber));
      }
    }
  }
}

int main(int argc, char** argv)
{
  int fd,c, res;
  struct termios oldtio,newtio;

  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS11", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }
  
  llopen(fd, &oldtio, &newtio);

  // Receive start Trama -> Capture fileName and fileTotalSize


  // Receive Trama (I)
  printf("Starting Receive Trama (I)\n");
  
  int numPackets = 2; // Will be calculated in the future.
  llread(fd, numPackets);

  printf("END!\n");    

  sleep(1);

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
  return 0;
}
