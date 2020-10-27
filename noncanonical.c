/*Non-Canonical Input Processing*/

#include "headers/noncanonical.h"

volatile int STOP=FALSE;

unsigned char* startMessage;
off_t startMessageSize;

struct readReturn deStuffing(unsigned char * message, int size) {
  int currentMessageSize = 0;
  unsigned char* dataBytes = (unsigned char*) malloc(0);

  struct readReturn returnStruct;

  for(int i = 0; i < size; i++) {
    dataBytes = (unsigned char*) realloc(dataBytes, currentMessageSize + 1);

    // receiveMessage = (unsigned char*) realloc(receiveMessage, *receiveMessageSize + 1);

    // printf("message[%d] = %p\n", i, message[i]);
    if(message[i] == ESC) {
      if(i + 1 < size) {
        if(message[i+1] == ESC_XOR1) {
          dataBytes[currentMessageSize++] = FLAG_SET;
          // receiveMessage[(*receiveMessageSize)++] = FLAG_SET;
          // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
          i++;
        } else if(message[i+1] == ESC_XOR2) {
          dataBytes[currentMessageSize++] = ESC;
          // receiveMessage[(*receiveMessageSize)++] = ESC;
          // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
          i++;
        }
      } else {
        printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
        returnStruct.currentMessage = dataBytes;
      returnStruct.currentMessageSize = -1;
      return returnStruct;  
      }
    } else if(message[i] == FLAG_SET){
      printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
      returnStruct.currentMessage = dataBytes;
      returnStruct.currentMessageSize = -1;
      return returnStruct;  
    } else {
      dataBytes[currentMessageSize++] = message[i];
      // receiveMessage[(*receiveMessageSize)++] = message[i];
      // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
    }
    
    // printf("dataBytes[%d] = %p\n", currentMessageSize - 1, dataBytes[currentMessageSize - 1]);
  }

  // (*receiveMessageSize)--; // To Exclude BCC2
  memset(message, 0, size); // Clear array
  memcpy(message, dataBytes, currentMessageSize); // Substitute original array with destuffed content

  returnStruct.currentMessage = dataBytes;
  returnStruct.currentMessageSize = currentMessageSize;
  // free(dataBytes);
  
  return returnStruct;
}

struct receiveTramaReturn receiveTrama(int* nTrama, int fd) {
  unsigned char buf;
  char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC1_OK", "STOP" };
  int i = 0, res;
  bool repeatedByte = false;

  bool cFlag; // !nTrama = [Ns = 0 | 1]

  unsigned char* dataBytes = (unsigned char*) malloc(0);
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
        else {
          dataBytes = (unsigned char*) realloc(dataBytes, index + 1);
          dataBytes[index++] = buf;
        }
        break;  

      default: 
        if(strcmp(state[i], "BCC1_OK") == 0) {
          dataBytes = (unsigned char*) realloc(dataBytes, index + 1);
          dataBytes[index++] = buf;
        }
        else // Other_RCV
          i = 0;
    }
  } 

  if(cFlag == *nTrama) // repeatedByte
    repeatedByte = true;

  // Destuffing - Including BCC2
  
  struct receiveTramaReturn receiveTramaRet;
  
  struct readReturn deStuffingRet = deStuffing(dataBytes, index);
  receiveTramaRet.currentMessage = deStuffingRet.currentMessage;
  receiveTramaRet.currentMessageSize = deStuffingRet.currentMessageSize;

  if(deStuffingRet.currentMessageSize == -1) {
    receiveTramaRet.statusCode = -1;
    return receiveTramaRet; // PEDIR RETRANSMISSÃO (REJ)
  }
  
  // At this point dataBytes[index - 1] holds BCC2
  unsigned char bcc2 = computeBcc2(deStuffingRet.currentMessage, deStuffingRet.currentMessageSize - 1, 0); // Excluding BCC2
  free(dataBytes);
  
  if(bcc2 != deStuffingRet.currentMessage[deStuffingRet.currentMessageSize - 1]) {
    if(repeatedByte) {
      *nTrama = cFlag;
      receiveTramaRet.statusCode = 2;
      return receiveTramaRet; // Status Code for Repeated Byte -> Descartar campo de dados
    }
    else {
      receiveTramaRet.statusCode = -1;
      return receiveTramaRet; // Status Code Error -> PEDIR RETRANSMISSÃO (REJ)
    }
  }
  *nTrama = cFlag;
  if(repeatedByte) {
    receiveTramaRet.statusCode = 2;
    return receiveTramaRet; // Status Code for Repeated Byte -> Descartar campo de dados
  }
  receiveTramaRet.statusCode = 0;
  return receiveTramaRet;
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

bool checkEnd(unsigned char* endMessage, off_t endMessageSize) {
  int s = 1;
  int e = 1;
  if (startMessageSize != endMessageSize)
    return FALSE;
  else {
    if (endMessage[0] == C_END) {
      for (; s < startMessageSize; s++, e++)
      {
        if (startMessage[s] != endMessage[e])
          return FALSE;
      }
      return TRUE;
    }
    else
      return FALSE;
  }
  return true;
}

struct readReturn llread(int fd, int* tNumber) {
  int statusCode;

  struct receiveTramaReturn receiveRet;
  while(true) {
    // VALORES DE C são gerados consoante recebidos! CORRIGIR
    receiveRet = receiveTrama(tNumber, fd);
    if(receiveRet.statusCode == 0) {
      printf("\nReceived Trama with success!\n \nSendign RR (%d)\n", !(*tNumber));
      sendSupervisionTrama(fd, getCField("RR", !(*tNumber)));
      break;
    }
    else {
      printf("Didn't receive Trama with success!\n");
      if(receiveRet.statusCode == 2) {
        printf("\n-- Repeated Byte --\n\nSendign RR (%d)\n", !(*tNumber));
        sendSupervisionTrama(fd, getCField("RR",!(*tNumber)));
      }
      else if(receiveRet.statusCode == -1) { // Send REJ
        printf("\n-- Retransmit Byte --\n\nSendign REJ (%d)\n", *tNumber);
        sendSupervisionTrama(fd, getCField("REJ", *tNumber));
      }
    }
  }

  // for(int i = 0; i < currentMessageSize; i++) {
  //     printf("currentMessage[%d] = %p\n", i, currentMessage[i]);
  // }
  struct readReturn llreadRet;
  llreadRet.currentMessage = receiveRet.currentMessage;
  llreadRet.currentMessageSize = receiveRet.currentMessageSize - 1; // Exclude BCC2

  return llreadRet;
}

off_t sizeOfFile_Start(unsigned char *start)
{
  return (start[3] << 24) | (start[4] << 16) | (start[5] << 8) | (start[6]);
}

unsigned char *nameOfFile_Start(unsigned char *start)
{

  int L2 = (int)start[8];
  unsigned char *name = (unsigned char *)malloc(L2 + 1);

  int i;
  for (i = 0; i < L2; i++)
    name[i] = start[9 + i];

  name[L2] = '\0';
  return name;
}

void createFile(unsigned char *data, off_t* sizeFile, unsigned char filename[])
{
  FILE *file = fopen((char *)filename, "wb+");
  fwrite((void *)data, 1, *sizeFile, file);
  printf("%ld\n", *sizeFile);
  printf("New file created\n");
  fclose(file);
}

void llclose(int fd) {
  printf("\n-- SENT DISC --\n");
  sendSupervisionTrama(fd, getCField("DISC", true));
  receiveSupervisionTrama(false, getCField("UA", true), fd);
  printf("\n-- RECEIVED UA --\n");
}

int main(int argc, char** argv)
{
  int fd,c, res;
  struct termios oldtio,newtio;
  int tNumber = -1; // [Nr = 0 | 1]

  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS4", argv[1])!=0) && 
        (strcmp("/dev/ttyS11", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }
  
  llopen(fd, &oldtio, &newtio);

  // Receive start Trama -> Capture fileName and fileTotalSize
  struct readReturn startRet;

  startRet = llread(fd, &tNumber); // Only 1 Frame
  startMessage = startRet.currentMessage;
  startMessageSize = startRet.currentMessageSize;

  printf("\n-- RECEIVED START --\n");

  // Parse it's info
  off_t dataSize = sizeOfFile_Start(startMessage); // File Size

  unsigned char* fileName = nameOfFile_Start(startMessage); // File Name

  printf("-- File Size: %ld --\n", dataSize);
  printf("-- File Name: %s --\n", fileName);

  unsigned char* totalMessage = (unsigned char*) malloc(0);
  off_t totalMessageSize = 0;

  struct readReturn messageRet;

  while(true) {
    messageRet = llread(fd, &tNumber);
    if(checkEnd(messageRet.currentMessage, messageRet.currentMessageSize)) {
      printf("\n-- RECEIVED END --\n");
      break;
    }

    totalMessage = realloc(totalMessage, totalMessageSize + messageRet.currentMessageSize - DATA_HEADER_LEN);
    memcpy(&totalMessage[totalMessageSize], &messageRet.currentMessage[DATA_HEADER_LEN], messageRet.currentMessageSize - DATA_HEADER_LEN);
    totalMessageSize += (messageRet.currentMessageSize - DATA_HEADER_LEN);

    free(messageRet.currentMessage);
  }
    
  free(startMessage);
  createFile(totalMessage, &dataSize, "test.gif");
  free(totalMessage);

  llclose(fd);

  printf("END!\n");    
  
  sleep(1);

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
  return 0;
}
