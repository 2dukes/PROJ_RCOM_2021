#define FLAG_SET 0X7E
#define A_C_SET 0x03
#define Other_A 0x01

#define C_I( n ) ( (n == 0) ? 0x00 : 0x40 )

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define N_BYTES_FLAGS 6 // Número de bytes pertencentes a FLAGS na Trama de Informação
#define N_BYTES_TO_SEND 128

#define MAX_RETR 3
#define TIMEOUT 3

#define C_SET 0x03
#define C_DISC 0x0B
#define C_UA 0x07
#define C_RR( n )  ( (n) ? 0x85 : 0x05 )
#define C_REJ( n ) ( (n) ? 0x81 : 0x01 )

#define ESC 0x7D
#define ESC_XOR1 0X5E
#define ESC_XOR2 0x5D

#define DATA_HEADER_LEN 4
#define MODULE 256

#define C_DATA 0x01
#define C_START 0x02
#define C_END 0x03
#define T1 0x00
#define T2 0x01
#define L1 0x04

#define MAX_NUM_OCTETS 65536 // 2^16

#define MIN(a, b) ( (a < b) ? a : b)
