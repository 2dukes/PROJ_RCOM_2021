/*Non-Canonical Input Processing*/

#include "headers/writer.h"

int numRetries = 0;

bool readSuccessful = false;
int fd;

unsigned char toSend[N_BYTES_FLAGS + (N_BYTES_TO_SEND * 2)]; // Buffer da mensagem a enviar
int finalIndex;

void setHandler(int sigNum) {

  if(numRetries < MAX_RETR) {
    if(readSuccessful) 
      return;
    
    printf("\n--- Sending SET: Retry %d ---\n", numRetries);
    sendSupervisionTrama(fd, getCField("SET", false), A_C_SET);
    alarm(TIMEOUT);
    numRetries++;
  } 
  else {
    printf("Can't connect to Receptor! (After %d tries)\n", MAX_RETR);
    exit(1);
  }
}

void resendDISC(int signum) {
  if(numRetries < MAX_RETR) {
    if(readSuccessful) 
      return;
    
    printf("\n--- Sending DISC: Retry %d ---\n", numRetries);
    sendSupervisionTrama(fd, getCField("DISC", false), A_C_SET);
    alarm(TIMEOUT);
    numRetries++;
  } 
  else {
    printf("Can't connect to Receptor! (After %d tries)\n", MAX_RETR);
    exit(1);
  }
}

void resendTrama(int sigNum) {
  if(numRetries < MAX_RETR) {
    if(readSuccessful) 
      return;
    
    printf("\nSending Trama: Retry %d\n", numRetries);
    write(fd, toSend, finalIndex + 1);

    alarm(TIMEOUT);
    numRetries++;
  }
  else {
    printf("After %d tries to resend trama, it didn't work. Exiting program!\n", MAX_RETR);
    exit(1);
  }
}

int byteStuffing(unsigned char* dataToSend, int size) {
  int pos = 0, startingPos = 4;
  int index = 0, bytesTransformed = 0;

  unsigned char auxToSend[N_BYTES_TO_SEND * 2];

  while(index < size) {
    if(dataToSend[startingPos] == FLAG_SET) { // Há mais casos -> Slide 13
      auxToSend[pos++] = ESC;
      auxToSend[pos] = ESC_XOR1; 
      bytesTransformed++;
    }
    else if (dataToSend[startingPos] == ESC) {
      auxToSend[pos++] = ESC;
      auxToSend[pos] = ESC_XOR2; 
      bytesTransformed++;
    }
    else 
      auxToSend[pos] = dataToSend[startingPos];
    
    bytesTransformed++;
    pos++;
    startingPos++;
    index++;
  }

  // Copy "stuffed" array into toSend
  memcpy(&toSend[4], auxToSend, bytesTransformed);
  return 4 + bytesTransformed;
}

void llopen(struct termios* oldtio, struct termios* newtio) {
  configureSerialPort(fd, oldtio, newtio);

  // SEND SET
  printf("\n--- Sending SET ---\n");
  sendSupervisionTrama(fd, getCField("SET", false), A_C_SET);
  
  // RECEIVE UA
  readSuccessful = receiveSupervisionTrama(true, getCField("UA", false), fd, A_C_SET) == 1;
  printf("\n--- RECEIVED UA ---\n");

}

void varyBCC1_BCC2(unsigned char* message, int messageSize, bool* nTrama) {
  if(ERROR_MODE == 2) {
    unsigned char *copyToSend = (unsigned char *) malloc (messageSize);
    memcpy(copyToSend, message, messageSize);
    // BCC2
    int r = (rand() % 100) + 1;
    if(r <= BCC2_ERROR_PERCENTAGE) {
      int i = (rand() % (messageSize - 5)) + 4; // We don't want to change the FLAG
      unsigned char randomLetter = (unsigned char) ('A' + (rand() % 26));
      copyToSend[i] = randomLetter;
      printf("\n-- BCC2 Changed! --\n");
    }

    // BCC1
    r = (rand() % 100) + 1;
    if(r <= BCC1_ERROR_PERCENTAGE) {
      int i = (rand() % 3) + 1;
      unsigned char randomLetter = (unsigned char) ('A' + (rand() % 26));
      copyToSend[i] = randomLetter;
      printf("\n-- BCC1 Changed! --\n");
    }

    printf("\nSENT TRAMA (%d)!\n", *nTrama);
    write(fd, copyToSend, messageSize);
    free(copyToSend);
  }
}

void llwrite(unsigned char *buf, off_t size, bool* nTrama) {
    int i = 0;
    int j;
    int nTramasSent = 0;
    int nBytesRead = 0;

    while(i < size) {
      j = 4;
    
      toSend[0] = FLAG_SET; // F
      toSend[1] = A_C_SET; // A
      toSend[2] = C_I(*nTrama); // C
      toSend[3] = A_C_SET ^ toSend[2]; // BCC1
      
      nBytesRead = 0;
      while(nBytesRead < N_BYTES_TO_SEND && i < size) {
        toSend[j] = buf[i];
        
        nBytesRead++;
        i++;
        j++;
      }   
      
      toSend[j] = computeBcc2(buf, nBytesRead, nTramasSent * N_BYTES_TO_SEND);      
      finalIndex = byteStuffing(toSend, nBytesRead + 1); // +1 because of BCC2    
      toSend[finalIndex] = FLAG_SET;

      if(ERROR_MODE == 2)
        varyBCC1_BCC2(toSend, finalIndex + 1, nTrama);
      else {
        printf("\nSENT TRAMA (%d)!\n", *nTrama);
        write(fd, toSend, finalIndex + 1);
      };
      
      readSuccessful = false;
      int returnState = receiveSupervisionTrama(true, getCField("RR", !(*nTrama)), fd, A_C_SET); // [Nr = 0 | 1]
      readSuccessful = true;
      numRetries = 0;

      if(returnState == 1) {
        printf("\nReceiving RR\n");
        nTramasSent++;
        *nTrama = !(*nTrama);   
      }
      else if(returnState == 2) { // Retransmissão da última trama enviada
        printf("\nReceiving REJ\n");
        i -= nBytesRead;
      }
    }
}

unsigned char* openReadFile(char* fileName, off_t* fileSize) {
  FILE* f;
  struct stat metadata;
  unsigned char* fileData;

  if((f = fopen(fileName, "rb")) == NULL) {
    perror("Error opening file!");
    exit(-1);
  }
  // off_t st_size
  stat(fileName, &metadata);
  *fileSize = metadata.st_size;
  printf("File %s has %ld bytes!\n", fileName, *fileSize);

  fileData = (unsigned char *) malloc(*fileSize); // fileData will hold the file bytes -> DON'T FORGET TO FREE MEMORY AT THE END!
  fread(fileData, sizeof(unsigned char), *fileSize, f);

  return fileData;
}

unsigned char* buildControlTrama(char* fileName, off_t* fileSize, unsigned char controlField, off_t* packageSize) {
  int fileNameSize = strlen(fileName); // Size in bytes (chars)
  int sizeControlPackage = 5 * sizeof(unsigned char) + L1 * sizeof(unsigned char) + fileNameSize * sizeof(unsigned char);
  
  unsigned char* package = (unsigned char*) malloc(sizeControlPackage);
  package[0] = controlField;
  package[1] = T1;
  package[2] = L1;
  package[3] = ((*fileSize) >> 24) & 0xFF;
  package[4] = ((*fileSize) >> 16) & 0xFF;
  package[5] = ((*fileSize) >> 8) & 0xFF;
  package[6] = ((*fileSize)) & 0xFF;
  package[7] = T2;
  package[8] = fileNameSize;
  
  for(int i = 0; i < fileNameSize; i++) 
    package[9 + i] = fileName[i];
  
  *packageSize = 9 + fileNameSize;

  checkMaxBytesToSend(packageSize);

  return package;
}

unsigned char* encapsulateMessage(unsigned char* messageToSend, off_t* messageSize, off_t* totalMessageSize, int* numPackets) {
  unsigned char* totalMessage = (unsigned char*) malloc(0);
  
  int16_t aux;
  off_t auxBytesToSend = 0;
  int8_t sequenceNum = 0;

  for(off_t i = 0; i < *messageSize; i++) {
    if(i == auxBytesToSend) { // Each packet has N_BYTES_TO_SEND bytes of Info
      totalMessage = (unsigned char *) realloc(totalMessage, *totalMessageSize + DATA_HEADER_LEN);
      totalMessage[(*totalMessageSize)++] = C_DATA;                       // C
      totalMessage[(*totalMessageSize)++] = sequenceNum;                  // N
      aux = MIN((N_BYTES_TO_SEND - DATA_HEADER_LEN), (*messageSize - i)); // MIN(Number of Bytes we can send, remaining bytes)
      totalMessage[(*totalMessageSize)++] = (aux >> 8) & 0xFF; // L2
      totalMessage[(*totalMessageSize)++] = (aux) & 0xFF;      // L1

      sequenceNum = (sequenceNum + 1) % MODULE; // [0 - 255]
      auxBytesToSend += aux;
      (*numPackets)++;
    }  
    totalMessage = (unsigned char *) realloc(totalMessage, *totalMessageSize + 1);
    totalMessage[(*totalMessageSize)++] = messageToSend[i];
  }

  free(messageToSend);
  
  return totalMessage;
}

void llclose() {
  printf("\n-- SENT DISC --\n");
  sendSupervisionTrama(fd, getCField("DISC", true), A_C_SET);
  (void) signal(SIGALRM, resendDISC);
  numRetries = 0;
  readSuccessful = false;
  receiveSupervisionTrama(true, getCField("DISC", true), fd, Other_A);
  printf("\n-- RECEIVED DISC --\n");
  readSuccessful = true;
  printf("\n-- SENT UA --\n");
  sendSupervisionTrama(fd, getCField("UA", true), Other_A);
}

int main(int argc, char** argv)
{
  srand(time(NULL));
  (void) signal(SIGALRM, setHandler);
  bool nTrama = false; // [Ns = 0 | 1]

  struct termios oldtio,newtio;
  
  if ( (argc < 3) || 
        ((strcmp("/dev/ttyS10", argv[1])!=0) && 
        (strcmp("/dev/ttyS0", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort filename\n\tex: nserial /dev/ttyS1 pinguim.gif\n");
    exit(1);
  }

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  off_t fileSize;
  unsigned char *msg = openReadFile(argv[2], &fileSize);

  struct timespec startTime;
  clock_gettime(CLOCK_REALTIME, &startTime);

  llopen(&oldtio, &newtio);
  
  (void) signal(SIGALRM, resendTrama); // Registering a new SIGALRM handler

  // START -> Construct
  off_t startPackageSize = 0;
  unsigned char* startPackage = buildControlTrama(argv[2], &fileSize, C_START, &startPackageSize);
  printf("\nSTART Frame size: %ld\n", startPackageSize);

  // START -> Send
  printf("\n-- SENT START --\n");
  llwrite(startPackage, startPackageSize, &nTrama);

  numRetries = 0;

  unsigned char* totalMessage;
  off_t totalMessageSize = 0;
  int numPackets;
  totalMessage = encapsulateMessage(msg, &fileSize, &totalMessageSize, &numPackets);

  // Trama de Informação para o Recetor
  
  printf("numPackets to Send: %d - \n", numPackets);
  llwrite(totalMessage, totalMessageSize, &nTrama);
  
  free(totalMessage);

  // END -> Construct
  off_t endPackageSize = 0;
  unsigned char* endPackage = buildControlTrama(argv[2], &fileSize, C_END, &endPackageSize);
  printf("\nEND Frame size: %ld\n", endPackageSize);

  // END -> Send
  printf("\n-- SENT END --\n");
  llwrite(endPackage, endPackageSize, &nTrama);

  llclose();

  struct timespec endTime;
  clock_gettime(CLOCK_REALTIME, &endTime);
  double sTime = startTime.tv_sec + startTime.tv_nsec * 1e-9;
  double eTime = endTime.tv_sec + endTime.tv_nsec * 1e-9;
  printf("-Time Passed: %.6lf-\n", eTime - sTime);

  printf("\nEND!\n");

  sleep(1); 

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}


// HOW TO COMPILE!
// gcc noncanonical.c generalFunctions.c -o nC
// gcc writenoncanonical.c generalFunctions.c -o writeNC


