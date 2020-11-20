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

void parseArguments(char* argv, char* user, char* password, char* host, char* urlPath) {
	// User
	printf("%s\n", argv);
	char* colon = strchr(&argv[4], ':');
	int colonIndex = colon - argv;
	memcpy(user, &argv[6], colonIndex - 6);
	user[colonIndex - 6] = '\0';
	printf("User: %s\n", user);

	// Password
	char* at = strchr(argv, '@');
	int atIndex = at - argv;
	memcpy(password, &argv[colonIndex + 1], atIndex - (colonIndex + 1));
	password[atIndex - (colonIndex + 1)] = '\0';
	printf("Password: %s\n", password);

	// Host
	char* hostSlash = strchr(&argv[6], '/');
	int slashIndex = hostSlash - argv;
	memcpy(host, &argv[atIndex + 1], slashIndex - (atIndex + 1));
	host[slashIndex - (atIndex + 1)] = '\0';
	printf("Host: %s\n", host);

	// url-path
	char* urlPathSlash = strchr(&argv[6], '/') + 1;
	int pathIndex = urlPathSlash - argv;
	int urlPathLen = strlen(argv) - pathIndex ;
	memcpy(urlPath, &argv[slashIndex + 1], urlPathLen);
	urlPath[urlPathLen] = '\0';
	printf("Url Path: %s\n", urlPath);
}

int main(int argc, char** argv){
	// ftp://[<user>:<password>@]<host>/<url-path>

	char user[MAX_STRING_LENGTH];
	char password[MAX_STRING_LENGTH];
	char host[MAX_STRING_LENGTH]; 
	char urlPath[MAX_STRING_LENGTH];

	// url-path
	parseArguments(argv[1], user, password, host, urlPath);
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


