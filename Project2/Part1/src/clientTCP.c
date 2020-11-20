/*      (C)2000 FEUP  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>

#define SERVER_PORT 6000
#define SERVER_ADDR "192.168.28.96"
#define MAX_STRING_LENGTH 64

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "123"
#define URL_HEADER_LEN 6

void errorMessage(char* message, int statusCode) {
	printf("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n");
	exit(statusCode);
}

void parseArguments(char* argv, char* user, char* password, char* host, char* urlPath) {
	int atIndex;

	char* colon = strchr(&argv[4], ':');
	char* at = strchr(argv, '@');
	char* hostSlash = strchr(&argv[URL_HEADER_LEN], '/');
	char* urlPathSlash = strchr(&argv[URL_HEADER_LEN], '/') + 1;
	
	if(colon != NULL && at != NULL) {
		if(colon >= at)
			errorMessage("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n", 1);

		// User
		int colonIndex = colon - argv;
		memcpy(user, &argv[URL_HEADER_LEN], colonIndex - URL_HEADER_LEN);
		user[colonIndex - URL_HEADER_LEN] = '\0';

		// Password
		atIndex = at - argv;
		memcpy(password, &argv[colonIndex + 1], atIndex - (colonIndex + 1));
		password[atIndex - (colonIndex + 1)] = '\0';
	}
	else {
		strcpy(user, DEFAULT_USER);
		strcpy(password, DEFAULT_PASSWORD);
		atIndex = 5;
	}

	char* header;
	strncpy(header, argv, URL_HEADER_LEN);
	if(hostSlash == NULL || urlPathSlash == NULL || strcmp(header, "ftp://") != 0) 
		errorMessage("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n", 1);

	// Host
	int slashIndex = hostSlash - argv;
	memcpy(host, &argv[atIndex + 1], slashIndex - (atIndex + 1));
	host[slashIndex - (atIndex + 1)] = '\0';

	// url-path
	int pathIndex = urlPathSlash - argv;
	int urlPathLen = strlen(argv) - pathIndex ;
	memcpy(urlPath, &argv[slashIndex + 1], urlPathLen);
	urlPath[urlPathLen] = '\0';
}

int main(int argc, char** argv){
	// ftp://[<user>:<password>@]<host>/<url-path>
	// ftp://ftp.up.pt/pub/README.html
	// ftp://up201806441:123@ftp.up.pt/pub/README.html

	char user[MAX_STRING_LENGTH];
	char password[MAX_STRING_LENGTH];
	char host[MAX_STRING_LENGTH]; 
	char urlPath[MAX_STRING_LENGTH];
	parseArguments(argv[1], user, password, host, urlPath);

	printf("%s\n", argv[1]);
	printf("User: %s\n", user);
	printf("Password: %s\n", password);
	printf("Host: %s\n", host);
	printf("Url Path: %s\n", urlPath);

	exit(1);

	int	sockfd;
	struct	sockaddr_in server_addr;
	char	buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";  
	int	bytes;
	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(SERVER_PORT);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
	/*connect to the server*/
    	if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}
    	/*send a string to the server*/
	bytes = write(sockfd, buf, strlen(buf));
	printf("Bytes escritos %d\n", bytes);

	close(sockfd);
	exit(0);
}


