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
#include "macros.h"
#include <string.h>


void errorMessage(char* message, int statusCode) {
	printf("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n");
	exit(statusCode);
}

struct FTPclientArgs parseArguments(char* argv) {
	int atIndex;

	struct FTPclientArgs clientArgs;

	char* colon = strchr(&argv[4], ':');
	char* at = strchr(argv, '@');
	char* hostSlash = strchr(&argv[URL_HEADER_LEN], '/');
	char* urlPathSlash = strchr(&argv[URL_HEADER_LEN], '/') + 1;

	if(colon != NULL && at != NULL) {
		if(colon >= at)
			errorMessage("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n", 1);

		// User
		int colonIndex = colon - argv;
		clientArgs.user = (char *) malloc((colonIndex - URL_HEADER_LEN) + 1);
		memcpy(clientArgs.user, &argv[URL_HEADER_LEN], colonIndex - URL_HEADER_LEN);
		clientArgs.user[colonIndex - URL_HEADER_LEN] = '\0';


		// Password
		atIndex = at - argv;
		clientArgs.password = (char *) malloc((atIndex - (colonIndex + 1)) + 1);
		memcpy(clientArgs.password, &argv[colonIndex + 1], atIndex - (colonIndex + 1));
		clientArgs.password[atIndex - (colonIndex + 1)] = '\0';
	}
	else {
		clientArgs.user = (char *) malloc(11);
		strcpy(clientArgs.user, DEFAULT_USER);
		clientArgs.password = (char *) malloc(4);
		strcpy(clientArgs.password, DEFAULT_PASSWORD);
		atIndex = 5;
	}

	char header[7];
	strncpy(header, argv, URL_HEADER_LEN);
	if(hostSlash == NULL || urlPathSlash == NULL || strcmp(header, "ftp://") != 0) 
		errorMessage("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n", 1);

	// Host
	int slashIndex = hostSlash - argv;
	clientArgs.host = (char *) malloc((slashIndex - (atIndex + 1)) + 1);
	memcpy(clientArgs.host, &argv[atIndex + 1], slashIndex - (atIndex + 1));
	clientArgs.host[slashIndex - (atIndex + 1)] = '\0';

	// url-path
	int pathIndex = urlPathSlash - argv;
	int urlPathLen = strlen(argv) - pathIndex ;
	clientArgs.urlPath = (char *) malloc(urlPathLen + 1);
	memcpy(clientArgs.urlPath, &argv[slashIndex + 1], urlPathLen);
	clientArgs.urlPath[urlPathLen] = '\0';

	return clientArgs;
}

struct hostent getIP(char* hostName)
{
	struct hostent* h;
					
	/*
	struct hostent {
		char    *h_name;	Official name of the host. 
		char    **h_aliases;	A NULL-terminated array of alternate names for the host. 
		int     h_addrtype;	The type of address being returned; usually AF_INET.
		int     h_length;	The length of the address in bytes.
		char    **h_addr_list;	A zero-terminated array of network addresses for the host. 
					Host addresses are in Network Byte Order. 
	};

		#define h_addr h_addr_list[0]	The first address in h_addr_list. 
	*/
	if ((h = gethostbyname(hostName)) == NULL) {  
		herror("gethostbyname");
		exit(1);
	}

	printf("Host name  : %s\n", h->h_name);
	printf("IP Address : %s\n",inet_ntoa(*((struct in_addr *)h->h_addr)));

	return *h;
}


int main(int argc, char** argv) {
	// ftp://[<user>:<password>@]<host>/<url-path>
	// ftp://ftp.up.pt/pub/README.html
	// ftp://up201806441:123@ftp.up.pt/pub/README.html

	struct FTPclientArgs clientArgs = parseArguments(argv[1]);

	printf("%s\n", argv[1]);
	printf("User: %s\n", clientArgs.user);
	printf("Password: %s\n", clientArgs.password);
	printf("Host: %s\n", clientArgs.host);
	printf("Url Path: %s\n", clientArgs.urlPath);


	struct hostent hostInfo = getIP(clientArgs.host);

	int	sockfd;
	struct	sockaddr_in server_addr;
	int	bytes;
	
	/*server address handling*/
	/*
		struct sockaddr_in {
			short int sin_family; // Address family, AF_INET
			unsigned short int sin_port; // Port number
			struct in_addr sin_addr; // Internet address
			unsigned char sin_zero[8]; // Same size as struct sockaddr
		};

		struct in_addr {
			uint32_t s_addr; // that's a 32-bit int (4 bytes)
		};

		struct sockaddr {
			u_short sa_family; // Address family - ex: AF_INET
			char sa_data[14]; // Protocol address
		};
	*/

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
	sleep(2);
	char buf[2];
	while(1) {
		bytes = read(sockfd, buf, 1);
		printf("%c | %d\n", buf[0], buf[0]);
	}
	

	free(clientArgs.user);
	free(clientArgs.password);
	free(clientArgs.host);
	free(clientArgs.urlPath);
	close(sockfd);
	exit(0);
}


