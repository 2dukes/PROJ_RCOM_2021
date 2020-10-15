#define FLAG_SET 0X7E
#define A_C_SET 0x03

#define C_I( n ) ( (n == 0) ? 0x00 : 0x40 )

#define BUF_MAX_SIZE 256

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define N_BYTES_FLAGS 6 // Número de bytes pertencentes a FLAGS na Trama de Informação
#define N_BYTES_TO_SEND 5

#define MAX_RETR 3
#define TIMEOUT 3

#define C_SET 0x03
#define C_DISC 0x0B
#define C_UA 0x07
#define C_RR( n )  ( (n) ? 0x85 : 0x05 )
#define C_REJ( n ) ( (n) ? 0x81 : 0x01 )
