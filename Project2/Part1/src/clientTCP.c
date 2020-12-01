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
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>


void errorMessage(char* error, int statusCode) {
	printf(error);
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

		// Password
		atIndex = at - argv;
		clientArgs.password = (char *) malloc((atIndex - (colonIndex + 1)) + 1);
		memcpy(clientArgs.password, &argv[colonIndex + 1], atIndex - (colonIndex + 1));
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

	// url-path
	int pathIndex = urlPathSlash - argv;
	int urlPathLen = strlen(argv) - pathIndex ;
	clientArgs.urlPath = (char *) malloc(urlPathLen + 1);
	memcpy(clientArgs.urlPath, &argv[slashIndex + 1], urlPathLen);

	// filename
	char* filenameSlash = strrchr(argv, '/');
	int filenameSlashIndex = (filenameSlash - argv);
	clientArgs.filename = (char *) malloc(strlen(argv) - filenameSlashIndex);
	memcpy(clientArgs.filename, &argv[filenameSlashIndex + 1], strlen(argv) - filenameSlashIndex - 1); 
	
	if(strcmp(clientArgs.user, "") == 0 || strcmp(clientArgs.password, "") == 0 || 
		strcmp(clientArgs.host, "") == 0 || strcmp(clientArgs.urlPath, "") == 0 || 
		strcmp(clientArgs.filename, "") == 0)
		errorMessage("Usage: ./client ftp://[<user>:<password>@]<host>/<url-path>\n", 1);

	return clientArgs;
}

struct hostent* getIP(char* hostName)
{
	struct hostent* h = (struct hostent *) malloc(sizeof(struct hostent));

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

	return h;
}

enum readState getState(char character, enum readState previousState) {
	if(isdigit(character) && (previousState == LineChange || previousState == StatusCode))
		return StatusCode;
	else if(character == '\n') {
		if(previousState == Space || previousState == MainMsgText) 
			return EndMessage;
		return LineChange;
	}
	else if(character == ' ') {
		if(previousState == StatusCode)
			return Space;
		if(previousState == MainMsgText)
			return MainMsgText;	
		return DummyMsgText;
	}
	else if(character == '-')
		if(previousState == MainMsgText)
			return previousState;
		else return Dash;
	else {
		if(previousState == Space || previousState == MainMsgText)
			return MainMsgText;
		return DummyMsgText;
	}
}

// https://stackoverflow.com/questions/29152359/how-to-read-multiline-response-from-ftp-server
char* readResponse(int sockfd, char* statusCode) {
	enum readState rStatus = LineChange;
	// enum readState { StatusCode, Space, Dash, LineChange, DummyMsgText, MainMsgText, EndMessage};
	char c;
	int statusCodeIndex = 0, msgIndex = 0;
	char* message = (char *) malloc(0);

	while(rStatus != EndMessage) {
		read(sockfd, &c, 1);
		rStatus = getState(c, rStatus);
		printf("%c | %d\n", c, rStatus);
		switch (rStatus)
		{
			case StatusCode:
				statusCode[statusCodeIndex++] = c; 
				break;
			case LineChange:
				statusCodeIndex = 0;
				break;
			case MainMsgText:
				message = (char* ) realloc(message, msgIndex + 1);
				message[msgIndex++] = c;
			default:
				break;
		}
	}

	printf("Status Code: %s\n", statusCode);
	printf("\n-- Main Message: %s\n", message);
	if(strcmp(message, "\r") != 0) {
		printf("[%s] < %s\n", statusCode, message);
		fflush(stdout);
	}

	return message;
}

void sendCmd(int sockfd, char mainCMD[], char* contentCMD) {
	write(sockfd, mainCMD, strlen(mainCMD));
	write(sockfd, contentCMD, strlen(contentCMD));
	write(sockfd, "\n", 1);
	printf("> %s%s\n", mainCMD, contentCMD);
}


void readServerData(int dataSockfd, char* filename) {
	FILE* file = fopen(filename, "wb+");

	char c;
	while(read(dataSockfd, &c, 1) > 0) {
		fwrite(&c, 1, 1, file);
	}

	fclose(file);
}

// https://en.wikipedia.org/wiki/List_of_FTP_server_return_codes
int sendCommandAndFetchResponse(int sockfd, char mainCMD[], char* contentCMD, char* filename, int dataSockfd) {
	sendCmd(sockfd, mainCMD, contentCMD);

	while(true) {
		char statusCode[4];
		char* response = readResponse(sockfd, statusCode);
		switch (statusCode[0])
		{
			// Expect Another Reply
			case '1': {
				if(strcmp(mainCMD, "RETR ") == 0) 
					readServerData(dataSockfd, filename);
				
				break;
			}
			// Request Completed.
			case '2':
				return 2;
			
			// Needs further information (Login Case)
			case '3':
				return 3;
			
			// Command not accepted, but we can send it again.
			case '4':
				sendCmd(sockfd, mainCMD, contentCMD);
				break;

			// Command had errors!
			case '5': {
				int errorStatus;
				sscanf(statusCode, "%d", &errorStatus);
				exit(errorStatus);
			}
			default: 
				break;
		}
		free(response);
	}

}

// Parse Passive Mode String in order to fetch Port and IP Address.
char* parsePassiveModeArgs(char* response, int* port) {
	// (193,137,29,15,202,40) -> 193.137.29.15 | 202 * 256 + 40
	int nums[6];
	sscanf(response, "Entering Passive Mode (%d,%d,%d,%d,%d,%d).",&nums[0],&nums[1],&nums[2],&nums[3],&nums[4],&nums[5]);

	char* ip_address = (char *) malloc(0);
	int ip_index = 0;
	char aux[5];
	int len;

	for (int i = 0; i < 4; i++) {
		sprintf(aux, "%d", nums[i]);
		len = strlen(aux);

		ip_address = (char* ) realloc(ip_address, ip_index + len + 1);

		memcpy(&ip_address[ip_index], aux, len);

		ip_address[ip_index + len] = '.';
		ip_index += len + 1; 
	}

	*port = nums[4] * 256 + nums[5];
	ip_address[ip_index-1] = '\0';

	printf("Ip Address: %s\n", ip_address);
	printf("Port: %d\n\n\n", *port);

	return ip_address;
}

int openSocketAndConnect(struct hostent* hostInfo, uint16_t serverPort) {
	int	sockfd;
	struct sockaddr_in server_addr;
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
			char sa_data[14]; // Protocol address - sa_data contains a destination address and port number for the socket
		};
	*/

	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = *((struct in_addr *)hostInfo->h_addr);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(serverPort);		/*server TCP port must be network byte ordered */
    	
	/*open an TCP socket*/
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) 
		errorMessage("socket() failed.", 2);

	/*connect to the server*/
    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		errorMessage("connect() failed.", 3);
	
	return sockfd;
}

int main(int argc, char** argv) {
	// ftp://[<user>:<password>@]<host>/<url-path>
	// ftp://ftp.up.pt/pub/README.html
	// ftp://up201806441:123@ftp.up.pt/pub/README.html

	struct FTPclientArgs clientArgs = parseArguments(argv[1]);

	printf("\n\n%s\n", argv[1]);
	printf("-----------------------------------------------\n");
	printf("User: %s\n", clientArgs.user);
	printf("Password: %s\n", clientArgs.password);
	printf("Host: %s\n", clientArgs.host);
	printf("Url Path: %s\n", clientArgs.urlPath);
	printf("Filename: %s\n", clientArgs.filename);

	struct hostent* hostInfo = getIP(clientArgs.host);
	printf("-----------------------------------------------\n\n");
	int sockfd = openSocketAndConnect(hostInfo, SERVER_PORT);
	int dataSockfd = -1;

	/* Read Server Welcoming message */
	char statusCode[4];
	char* response = readResponse(sockfd, statusCode);

	if(statusCode[0] != '2') {
		char errorMsg[50];
		sprintf(errorMsg, "Status [%s] : %s\n", statusCode, response);
		errorMessage(errorMsg, 4);
	} else
		printf("[%s] < Connection Established  [%s:%d]\n", statusCode, clientArgs.host, SERVER_PORT);

	free(response);
	int ret = sendCommandAndFetchResponse(sockfd, "USER ", clientArgs.user, clientArgs.filename, dataSockfd);
	if(ret == 3) 
		sendCommandAndFetchResponse(sockfd, "PASS ", clientArgs.password, clientArgs.filename, dataSockfd);
	
	write(sockfd, "PASV\n", 5);
	response = readResponse(sockfd, statusCode);

	int* port = malloc(sizeof(int));
	char* ipAdr = parsePassiveModeArgs(response, port);
	
	struct hostent* dataHostInfo = getIP(ipAdr);
	dataSockfd = openSocketAndConnect(dataHostInfo, *port);
	
	ret = sendCommandAndFetchResponse(sockfd, "RETR ", clientArgs.urlPath, clientArgs.filename, dataSockfd);
	if(ret == 2) 
		printf("> Finished file download!\n");
	else
		printf("> Error downloading file!\n");
	
	// free((void *) hostInfo);
	free(ipAdr);
	free(port);
	free(clientArgs.user);
	free(clientArgs.password);
	free(clientArgs.host);
	free(clientArgs.urlPath);
	close(sockfd);
	close(dataSockfd);
	exit(0);
}


