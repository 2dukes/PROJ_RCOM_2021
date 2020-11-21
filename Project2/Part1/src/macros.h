#define SERVER_PORT 21
#define SERVER_ADDR "193.137.29.15"

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "123"
#define URL_HEADER_LEN 6

struct FTPclientArgs {
    char* user;
    char* password;
    char* host;
    char* urlPath;
};