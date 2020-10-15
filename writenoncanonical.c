/*Non-Canonical Input Processing*/

#include "headers/writenoncanonical.h"

volatile int STOP=FALSE;

int numRetries = 0;
bool readSuccessful = false;
int fd;

void alarmHandler(int sigNum) {
  // Colocar aqui o código que deve ser executado
  if(numRetries < MAX_RETR) {
    if(readSuccessful)
      return;

    sendSupervisionTrama(fd, getCField("SET", false));
    alarm(TIMEOUT);
    numRetries++;
  } 
  else {
    printf("Can't connect to Receptor! (After %d tries)\n", MAX_RETR);
    exit(1);
  }
}

void sendData(bool c) {
    unsigned char toSend[N_BYTES_FLAGS + (N_BYTES_TO_SEND * 2)];

    toSend[0] = FLAG_SET; // F
    toSend[1] = A_C_SET; // A
    toSend[2] = C_I(c);
    toSend[3] = A_C_SET ^ toSend[2]; // BCC1
    
    unsigned char someRandomBytes[N_BYTES_TO_SEND * 2];
    
    // Data to Send
    someRandomBytes[0] = 0x0A;
    someRandomBytes[1] = 0x0B;
    someRandomBytes[2] = 0x0C;
    someRandomBytes[3] = 0x0D;
    someRandomBytes[4] = 0x0E;
    
    int i = 0;
    int j = 4;
    while(someRandomBytes[i] != '\0') {
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
      toSend[j] = someRandomBytes[i];
      
      i++;
      j++;
    }

    toSend[j] = computeBcc2(someRandomBytes, N_BYTES_TO_SEND);
    toSend[j + 1] = FLAG_SET;
    
    int res = write(fd, toSend, 5 + j + 2);   
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

    fd = configureSerialPort(argv[1], &oldtio, &newtio);

    printf("New termios structure set\n");

    // SEND SET
    sendSupervisionTrama(fd, getCField("SET", false));
    
    // RECEIVE UA
    readSuccessful = receiveSupervisionTrama(true, getCField("UA", false), fd);
    
    // Trama de Informação para o Recetor
    printf("\nSENT TRAMA!\n");

    bool tNumber = true; // [Ns = 0 | 1]
    sendData(tNumber);
    
    printf("\nReceiving RR\n");
    receiveSupervisionTrama(false, getCField("RR", !tNumber), fd); // [Nr = 0 | 1]

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


