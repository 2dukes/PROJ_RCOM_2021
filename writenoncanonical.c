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
        // if(someRandomBytes[i] == 0x7E) { // Há mais casos -> Slide 13
        //   toSend[j] = 0x7D;
        //   toSend[j + 1] = 0x5E; 
        //   j++;
        // }
        // else if (someRandomBytes[i] == 0x7D) {
        //   toSend[j] = 0x7D;
        //   toSend[j + 1] = 0x5D; 
        //   j++;
        // }
        // else
        toSend[j] = buf[i];
        
        nBytesRead++;
        i++;
        j++;
      }   

      toSend[j] = computeBcc2(buf, nBytesRead, nTramasSent * N_BYTES_TO_SEND);
      toSend[j + 1] = FLAG_SET;

      printf("\nSENT TRAMA (%d)!\n", nTrama);
      int res = write(fd, toSend, N_BYTES_FLAGS + nBytesRead);
      
      readSuccessfulFRAME = false;
      printf("\nReceiving RR / REJ\n");
      int returnState = receiveSupervisionTrama(true, getCField("RR", !nTrama), fd); // [Nr = 0 | 1]
      readSuccessfulFRAME = true;
      numRetries = 0;
      if(returnState != 1) // Retransmissão da última trama enviada
        i -= nBytesRead;
      else
        nTramasSent++;

      nTrama = !nTrama;   
    }
}

 

int main(int argc, char** argv)
{
  (void) signal(SIGALRM, alarmHandler);

  int c, res;
  struct termios oldtio,newtio;
  int i, sum = 0, speed = 0;
  
  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS10", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  llopen(&oldtio, &newtio);
  
  (void) signal(SIGALRM, resendTrama); // Registering a new SIGALRM handler

  // Trama de Informação para o Recetor
  numRetries = 0;

  // Data to Send
  unsigned char someRandomBytes[BUF_MAX_SIZE];
  someRandomBytes[0] = 0x0A;
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


