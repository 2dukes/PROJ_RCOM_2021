/*Non-Canonical Input Processing*/

#include "headers/noncanonical.h"

volatile int STOP=FALSE;

unsigned char* startMessage;
off_t startMessageSize;

int deStuffing(unsigned char * message, int size, unsigned char* receiveMessage, off_t* receiveMessageSize) {
  int currentMessageSize = 0;
  unsigned char dataBytes[N_BYTES_TO_SEND + 1];
  *receiveMessageSize = 0;

  for(int i = 0; i < size; i++) {
    // dataBytes = (unsigned char*) realloc(dataBytes, currentMessageSize + 1);

    // receiveMessage = (unsigned char*) realloc(receiveMessage, *receiveMessageSize + 1);

    // printf("message[%d] = %p\n", i, message[i]);
    if(message[i] == ESC) {
      if(i + 1 < size) {
        if(message[i+1] == ESC_XOR1) {
          dataBytes[currentMessageSize++] = FLAG_SET;
          receiveMessage[(*receiveMessageSize)++] = FLAG_SET;
          // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
          i++;
        } else if(message[i+1] == ESC_XOR2) {
          dataBytes[currentMessageSize++] = ESC;
          receiveMessage[(*receiveMessageSize)++] = ESC;
          // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
          i++;
        }
      } else {
        printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
        return -1;
      }
    } else if(message[i] == FLAG_SET){
      printf("\n--- String Malformed (Incorrect Stuffing)! ---\n");
      return -1;
    } else {
      dataBytes[currentMessageSize++] = message[i];
      receiveMessage[(*receiveMessageSize)++] = message[i];
      // printf("receiveMessage[%d] = %p\n", *receiveMessageSize - 1, receiveMessage[*receiveMessageSize - 1]);
    }
    
    // printf("dataBytes[%d] = %p\n", currentMessageSize - 1, dataBytes[currentMessageSize - 1]);
  }

  (*receiveMessageSize)--; // To Exclude BCC2
  memset(message, 0, size); // Clear array
  memcpy(message, dataBytes, currentMessageSize); // Substitute original array with destuffed content

  // free(dataBytes);
  
  return currentMessageSize;
}

int receiveTrama(int* nTrama, int fd, unsigned char* receivedMessage, off_t* receivedMessageSize) {
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
  int currentMessageSize = deStuffing(dataBytes, index, receivedMessage, receivedMessageSize);
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

void saveMessage(unsigned char* messageRead, off_t* messageReadSize, unsigned char* currentMessage, off_t currentMessageSize) {
  // printf("-%ld-\n", currentMessageSize);
  for(int i = 0; i < currentMessageSize; i++) {
    // messageRead = (unsigned char*) realloc(messageRead, *messageReadSize + 1);
    messageRead[*messageReadSize] = currentMessage[i];
    
    // printf("currentMessage[%d] = %p\n", i, currentMessage[i]);
    printf("messageRead[%ld] = %p\n", *messageReadSize, messageRead[*messageReadSize]);

    (*messageReadSize)++;
  }
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

off_t llread(int fd, int numPackets, unsigned char* messageRead) {
  int i = 0, statusCode;
  int tNumber = -1; // [Nr = 0 | 1]
  off_t currentMessageSize = 0;
  off_t messageReadSize = 0;

  // unsigned char* messageRead = (unsigned char *) malloc(0);
  unsigned char currentMessage[N_BYTES_TO_SEND + 1];

  while(i < numPackets) {
    currentMessageSize = 0;
    // VALORES DE C são gerados consoante recebidos! CORRIGIR
    statusCode = receiveTrama(&tNumber, fd, currentMessage, &currentMessageSize);
    if(statusCode == 0) {
      printf("\nReceived Trama %d with success!\n \nSendign RR (%d)\n", i, !tNumber);
      if(!checkEnd(currentMessage, currentMessageSize)) {
        saveMessage(messageRead, &messageReadSize, currentMessage, currentMessageSize);
        printf("ASDASDASDASD\n");
      }
      else 
        printf("-- RECEIVED END --\n");
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
  return messageReadSize;
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
  printf("%zd\n", *sizeFile);
  printf("New file created\n");
  fclose(file);
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
  startMessage = (unsigned char*) malloc(0);
  startMessageSize = llread(fd, 1, startMessage); // Only 1 Frame
  printf("\n-- RECEIVED START --\n");

  // Parse it's info
  off_t dataSize = sizeOfFile_Start(startMessage); // File Size
  unsigned char* fileName = nameOfFile_Start(startMessage); // File Name

  printf("-- File Size: %ld --\n", dataSize);
  printf("-- File Name: %s --\n", fileName);

  // Receive Trama (I)
  // printf("Starting Receive Trama (I)\n");
  

  int completePackets = (dataSize) / (N_BYTES_TO_SEND - DATA_HEADER_LEN);
  float remain = (float) dataSize / (N_BYTES_TO_SEND - DATA_HEADER_LEN) - completePackets;
  int lastBytes = remain * (N_BYTES_TO_SEND - DATA_HEADER_LEN);
  off_t sizeToAllocate = (completePackets * N_BYTES_TO_SEND) + lastBytes + DATA_HEADER_LEN;
  
  // printf("CompletePackets: %d\n", completePackets);
  // printf("Remain: %f\n", remain);
  // printf("LastBytes: %d\n", lastBytes);
  // printf("sizeToAllocate: %ld\n", sizeToAllocate);
  
  // 11324
  int numPackets = 89; // Will be calculated in the future.
  unsigned char* totalMessage = (unsigned char*) malloc(sizeToAllocate);
  llread(fd, numPackets, totalMessage);
  // printf("%ld\n", sizeToAllocate);

  printf("- SEPARATOR -\n");

  unsigned char* endMessage = (unsigned char*) malloc(startMessageSize);
  numPackets = 1;
  llread(fd, numPackets, endMessage);

  // for(int i = 0; i < N_BYTES_TO_SEND; i++) 
  //   printf("- %p -\n", totalMessage[i]);
  // createFile();

  free(startMessage);
  free(totalMessage);

  printf("END!\n");    
  
  sleep(1);

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  close(fd);
  return 0;
}
