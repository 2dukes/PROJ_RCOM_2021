/*Non-Canonical Input Processing*/

#include "headers/writenoncanonical.h"

volatile int STOP=FALSE;

int numRetries = 0;

bool readSuccessfulFRAME = false;
bool readSuccessfulSET = false;
int fd;

unsigned char toSend[N_BYTES_FLAGS + (N_BYTES_TO_SEND * 2)]; // Buffer da mensagem a enviar
int nBytesRead;

void alarmHandler(int sigNum) {
  // Colocar aqui o código que deve ser executado
  if(numRetries < MAX_RETR) {
    if(readSuccessfulSET) 
      return;
    
    printf("\n--- Sending SET: Retry %d ---\n", numRetries);
    sendSupervisionTrama(fd, getCField("SET", false));
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
    if(readSuccessfulFRAME) 
      return;
    
    printf("\nSending Trama: Retry %d\n", numRetries);
    write(fd, toSend, N_BYTES_FLAGS + nBytesRead);
    alarm(TIMEOUT);
    numRetries++;
  }
  else {
    printf("After %d tries to resend trama, it didn't work. Exiting program!\n", MAX_RETR);
    exit(1);
  }
}

void llopen(struct termios* oldtio, struct termios* newtio) {
  configureSerialPort(fd, oldtio, newtio);

  // SEND SET
  printf("\n--- Sending SET ---\n");
  sendSupervisionTrama(fd, getCField("SET", false));
  
  // RECEIVE UA
  readSuccessfulSET = receiveSupervisionTrama(true, getCField("UA", false), fd) == 1;
  printf("\n--- RECEIVED UA ---\n");

}

int byteStuffing(unsigned char* dataToSend, int size) {
  int pos = 0, startingPos = 4;
  int index = 0, bytesTransformed = 0;

  unsigned char auxToSend[N_BYTES_TO_SEND * 2];

  while(index < size) {
    if(dataToSend[startingPos] == FLAG_SET) { // Há mais casos -> Slide 13
      auxToSend[pos++] = ESC;
      auxToSend[pos] = ESC_XOR1; 
      bytesTransformed ++;
    }
    else if (dataToSend[startingPos] == ESC) {
      auxToSend[pos++] = ESC;
      auxToSend[pos] = ESC_XOR2; 
      bytesTransformed ++;
    }
    else 
      auxToSend[pos] = dataToSend[startingPos];
    
    bytesTransformed++;
    pos++;
    startingPos++;
    index++;
  }

  // for(int i = 0; i < bytesTransformed; i++)  
  //   printf("auxtoSend[%d] = %p\n", i, auxToSend[i]);

  // Copy "stuffed" array into toSend
  memcpy(&toSend[4], auxToSend, bytesTransformed);
  return 4 + bytesTransformed;
}

void llwrite(unsigned char buf[BUF_MAX_SIZE], int size) {
    int i = 0;
    int j;
    int nTramasSent = 0;
    bool nTrama = false; // [Ns = 0 | 1]

    while(i < size) {
      j = 4;
    
      toSend[0] = FLAG_SET; // F
      toSend[1] = A_C_SET; // A
      toSend[2] = C_I(nTrama); // C
      toSend[3] = A_C_SET ^ toSend[2]; // BCC1
      
      nBytesRead = 0;
      while(nBytesRead < N_BYTES_TO_SEND && i < size) {
        toSend[j] = buf[i];
        
        nBytesRead++;
        i++;
        j++;
      }   

      toSend[j] = computeBcc2(buf, nBytesRead, nTramasSent * N_BYTES_TO_SEND);      
      int finalIndex = byteStuffing(toSend, nBytesRead + 1); // +1 because of BCC2    
      toSend[finalIndex] = FLAG_SET;

      printf("\nSENT TRAMA (%d)!\n", nTrama);
      int res = write(fd, toSend, finalIndex + 1);
      
      readSuccessfulFRAME = false;
      printf("\nReceiving RR / REJ\n");
      int returnState = receiveSupervisionTrama(true, getCField("RR", !nTrama), fd); // [Nr = 0 | 1]
      readSuccessfulFRAME = true;
      numRetries = 0;

      if(returnState == 1) {
        nTramasSent++;
        nTrama = !nTrama;   
      }
      else if(returnState == 2) // Retransmissão da última trama enviada
        i -= nBytesRead;
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
  
  // for(int i = 0; i < *fileSize; i++) 
  //   printf("Byte[%d] = %p", i, fileData[i]);

  return fileData;
}

unsigned char* buildControlTrama(char* fileName, off_t* fileSize, unsigned char controlField) {
  int fileNameSize = strlen(fileName); // Size in bytes (chars)
  int sizeControlPackage = 5 * sizeof(unsigned char) + L1 * sizeof(unsigned char) + fileNameSize * sizeof(unsigned char);
  printf("SizeControlPackege = %d\n", sizeControlPackage);
  
  unsigned char* package = (unsigned char*) malloc(sizeControlPackage); // FREE MEMORY AT THE END
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
  
  return package;
}

int main(int argc, char** argv)
{
  (void) signal(SIGALRM, alarmHandler);

  int c, res;
  struct termios oldtio,newtio;
  int i, sum = 0, speed = 0;
  
  if ( (argc < 3) || 
        ((strcmp("/dev/ttyS10", argv[1])!=0) && 
        (strcmp("/dev/ttyS0", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort filename\n\tex: nserial /dev/ttyS1 pinguim.gif\n");
    exit(1);
  }

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  off_t fileSize;
  printf("Size: %d\n", sizeof(off_t));
  unsigned char *msg = openReadFile(argv[2], &fileSize);

  llopen(&oldtio, &newtio);
  
  (void) signal(SIGALRM, resendTrama); // Registering a new SIGALRM handler

  // Trama START
  unsigned char* startPackage = buildControlTrama(argv[2], &fileSize, C_START);

  // Enviar START

  // Trama de Informação para o Recetor
  numRetries = 0;

  // Data to Send
  unsigned char someRandomBytes[BUF_MAX_SIZE];
  someRandomBytes[0] = 0x7D;
  someRandomBytes[1] = 0xFB;
  someRandomBytes[2] = 0xCC;
  someRandomBytes[3] = 0xED;
  someRandomBytes[4] = 0x0E;
  someRandomBytes[5] = 0x0A;
  someRandomBytes[6] = 0xFB;
  someRandomBytes[7] = 0xCC;
  someRandomBytes[8] = 0xED;
  someRandomBytes[9] = 0X0E;
  
  // printf("Tamanho do buffer: %d\n", sizeof(someRandomBytes));
  llwrite(someRandomBytes, 10);

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


