/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

#define FLAG_SET 0X7E
#define A_C_SET 0x03
#define C_UA 0x07


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

    // RECEIVE SET

    char buf;
    char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC_OK", "STOP" };
    int i = 0;

    while (strcmp(state[i], "STOP") != 0) {       /* loop for input */
      printf("\nYEH BOY!\n");
      printf("\nSTATE: %s\n", state[i]);
      res = read(fd, &buf, 1);   /* returns after 1 chars have been input */
      printf("%p", buf);

      switch (buf)
      {
        case FLAG_SET:
          if(strcmp(state[i], "START") == 0 || strcmp(state[i], "BCC_OK") == 0)
            i++;
          else
            i = 1; // STATE = FLAG_RCV
          break;
        case A_C_SET:
          if(strcmp(state[i], "FLAG_RCV") == 0 || strcmp(state[i], "A_RCV") == 0)
            i++;
          else 
            i = 0; // Other_RCV
          break;
        case A_C_SET ^ A_C_SET: // BCC
          if(strcmp(state[i], "C_RCV") == 0)
            i++;
          else 
            i = 0; // Other_RCV
          break;
        default: // Other_RCV
          i = 0; // STATE = START
      }

    } 

    // SEND UA 

    unsigned char bytes[5];
    bytes[0] = FLAG_SET;
    bytes[1] = A_C_SET;
    bytes[2] = C_UA;
    bytes[3] = A_C_SET ^ C_UA;
    bytes[4] = FLAG_SET;

    res = write(fd, bytes, sizeof(bytes));   
    printf("\n%d bytes written\n", res);

    printf("END!\n");
    // finalStr[bytesRead] = '\0';
    // printf(":%s:\n", finalStr);

    // res = write(fd, finalStr, bytesRead);
    // printf("%d bytes written\n", bytesRead);


    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
