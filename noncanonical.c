/*Non-Canonical Input Processing*/

#include "headers/noncanonical.h"

volatile int STOP=FALSE;

int receiveTrama(bool nTrama, int fd) {
  unsigned char buf;
  char state[6][25] = { "START", "FLAG_RCV", "A_RCV", "C_RCV", "BCC1_OK", "STOP" };
  int i = 0, res;

  bool nTrama_Sender = !nTrama; // [Ns = 0 | 1]
  unsigned char cField = C_I(!nTrama);

  unsigned char dataBytes[BUF_MAX_SIZE];
  int index = 0;

  while (strcmp(state[i], "STOP") != 0) {       /* loop for input */
    printf("\nSTATE (R): %s\n", state[i]);
    res = read(fd, &buf, 1);   /* returns after 1 chars have been input */
    printf("%p\n", buf);

    // Special cases (as they use dynamic fields)
    if(buf == (A_C_SET ^ cField)) { // BCC1
      if(strcmp(state[i], "C_RCV") == 0) {
        i++;
        continue;
      }
    }
    else if(buf == cField) { // C
      if(strcmp(state[i], "A_RCV") == 0) {
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

      case A_C_SET: // A or BCC1 if [Ns = 0] 
        if(strcmp(state[i], "FLAG_RCV") == 0)
          i++;         
        else if(strcmp(state[i], "BCC1_OK") != 0)
          i = 0; // Other_RCV
        else
          dataBytes[index++] = buf;
        break;  

      default: // Other_RCV
        // printf("W: %p | %p | %p | %d\n", buf, A_C_SET ^ cField, cField, buf == cField);
        if(strcmp(state[i], "BCC1_OK") == 0) 
          dataBytes[index++] = buf;
        else
          i = 0;
    }
  } 

  // At this point dataBytes[index - 1] holds BCC2
  unsigned char bcc2 = computeBcc2(dataBytes, index - 1);
  if(bcc2 != dataBytes[index - 1])
    return -1; // Error
  return 0;
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

    fd = configureSerialPort(argv[1], &oldtio, &newtio);

    printf("New termios structure set\n");

    // RECEIVE SET
    receiveSupervisionTrama(false, getCField("SET", false), fd);

    // SEND UA 
    sendSupervisionTrama(fd, getCField("UA", false));

    // Receive Trama (I)
    printf("Starting Receive Trama (I)\n");
    
    bool tNumber = false; // [Nr = 0 | 1]
    if(receiveTrama(tNumber, fd) == 0)
      printf("\nAll good!\n");

    printf("Sendign RR\n");


    sendSupervisionTrama(fd, getCField("RR", tNumber));

    printf("END!\n");    

    sleep(1);

    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }
    close(fd);
    return 0;
}
