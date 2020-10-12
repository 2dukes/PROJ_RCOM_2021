/*Non-Canonical Input Processing*/

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

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

#define FLAG_SET 0X7E
#define A_C_SET 0x03
#define C_UA 0x07
 
#define MAX_RETR 3
#define TIMEOUT 3

int numRetries = 0;
bool finish = false;
bool readSuccessful = false;
int fd;

void send_SET() {
  int res;
  
  unsigned char buf[5];
  buf[0] = FLAG_SET;
  buf[1] = A_C_SET;
  buf[2] = A_C_SET;
  buf[3] = A_C_SET ^ A_C_SET;
  buf[4] = FLAG_SET;

  // printf("-%d-\n", sizeof(buf));
  printf("\nSending SET...\n");
  res = write(fd, buf, sizeof(buf));   
  printf("%d bytes written\n", res);
}

void alarmHandler(int sigNum) {
  // Colocar aqui o código que deve ser executado
  if(numRetries < MAX_RETR) {
    if(readSuccessful)
      return;

    send_SET();
    alarm(TIMEOUT);
    numRetries++;
  } 
  else {
    finish = true; // Unuseful
    printf("Can't connect to Receptor! (After %d tries)\n", MAX_RETR);
    exit(1);
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


    /*
      Open serial port device for reading and writing and not as controlling tty
      because we don't want to get killed if linenoise sends CTRL-C.
    */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

    /* 
      VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
      leitura do(s) pr�ximo(s) caracter(es)
    */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    // SEND SET
    send_SET();
    
    // RECEIVE UA

    alarm(TIMEOUT);

    char byte;
    char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC_OK", "STOP" };
    i = 0;

    while (strcmp(state[i], "STOP") != 0) {       /* loop for input */
      printf("\nSTATE: %s\n", state[i]);
      res = read(fd, &byte, 1);   /* returns after 1 chars have been input */
      
      if(res < 0 && !finish)  // Read interrupted by a signal
        continue; // Jumps to another iteration
      
      printf("%p", byte);

      switch (byte)
      {
        case FLAG_SET:
          if(strcmp(state[i], "START") == 0 || strcmp(state[i], "BCC_OK") == 0)
            i++;
          else
            i = 1; // STATE = FLAG_RCV
          break;
        case A_C_SET:
          if(strcmp(state[i], "FLAG_RCV") == 0)
            i++;
          else 
            i = 0; // Other_RCV
          break;
        case C_UA:
          if(strcmp(state[i], "A_RCV") == 0)
            i++;
          else
            i = 0;
          break;
        case A_C_SET ^ C_UA: // BCC
          if(strcmp(state[i], "C_RCV") == 0)
            i++;
          else 
            i = 0; // Other_RCV
          break;
        default: // Other_RCV
          i = 0; // STATE = START
      }
    }
    readSuccessful = true;
    
    printf("\nEND!\n");

    sleep(1); // p.e em caso de o write não ter terminado
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }


    close(fd);
    return 0;
}
