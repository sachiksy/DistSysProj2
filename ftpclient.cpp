#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <netdb.h>
#include <fcntl.h>

using namespace std;

#define BUFFER 1024

const char eof[] = "EOF";

void put_file(char* fname, int client_id) {
	//open file and send file status to server
	char *stats = "NULL";
	FILE* doc = fopen(fname, "rb");		//rb to check b/c with "r", file must exist or null
	
	//file DOES NOT exist, send file status, end function, return to processing user input
	if (doc == NULL) {
		//send file DOES NOT EXIST status
		if (send(client_id, stats, (int)strlen(stats), 0) < 0) {
			perror("ERROR: Failed to send file status to server.\n");
			close(client_id);
			exit(EXIT_FAILURE);
		}
		printf("%s DOES NOT EXIST in the LOCAL directory.\n", fname);
		printf("Cannot perform PUT <%s>.\n", fname);
		return;
	}
	//file EXISTs...
	else {
		printf("%s EXISTs in LOCAL directory.\n", fname);
		//send file EXISTs status
		if (send(client_id, fname, (int)strlen(fname), 0) < 0) {
			perror("ERROR: Failure to send file status to server.\n");
			fclose(doc);
			close(client_id);
			exit(EXIT_FAILURE);
		}
		
		char data[BUFFER];
		
		//receive GOOD TO GO signal
		if (recv(client_id, data, sizeof(data), 0) < 0) {
			perror("ERROR: Problems receiving GOOD TO GO signal.\n");
			fclose(doc);
			close(client_id);
			exit(EXIT_FAILURE);
		}
		
		size_t size;
		memset(data, '\0', BUFFER);
		
		//reading the file and sending it bytes to server
		do {
			size = fread(data, 1, sizeof(data), doc);
			//printf("The bytes read are %s\n", data); //tk
			
			//Problems with fread()
			if (size < 0) {
				perror("ERROR: Problems fread()ing file.\n");
				fclose(doc);
				close(client_id);
				exit(EXIT_FAILURE);
			}
			
			//sending bytes of file to server
			if (send(client_id, data, size, 0) < 0) {
				perror("ERROR: Cannot send file to server.\n");
				fclose(doc);
				close(client_id);
				exit(EXIT_FAILURE);
			}
		} while (size > 0);
		printf("DONE sending\n");
		
		//send EOF signal
		if (send(client_id, eof, sizeof(eof), 0) < 0) {
			perror("ERROR: Cannot send END OF FILE signal to server.\n");
			fclose(doc);
			close(client_id);
			exit(EXIT_FAILURE);
		}
		
		fclose(doc);
	}
}

int main(int argc, char *argv[]){
	int sock, i;
	struct addrinfo hints, *results, *j;
	char buf[BUFFER];

	//check for proper command args
	if (argc!=3){
		printf("Usage: %s <server hostname> <port number>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	//obtain address(es) matching hostname and port number
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;			//IPv4
	hints.ai_socktype = SOCK_STREAM;	//TCP socket
	
	//getaddrinfo(hostname, port number, address specifications, start of linked list of address structs)
	if (getaddrinfo(argv[1], argv[2], &hints, &results) < 0) {
		perror("ERROR: Cannot resolve the address.\n");
		exit(EXIT_FAILURE);
	}
	
	//try each address until successful connection
	for (j = results; j != NULL; j = j->ai_next) {
		//if socket creation fail, continue
		if ((sock = socket(j->ai_family, j->ai_socktype, j->ai_protocol)) < 0) {
			continue;
		}
		//connection success, break
		if (connect(sock, j->ai_addr, j->ai_addrlen) != -1) {
			break;
		}
		
		close(sock);
	}
	//no successful address
	if (j == NULL) {
		perror("ERROR: Cannot connect to address.\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(results);
	
	//user input handling
	while(strcmp(buf, "quit") != 0){
		printf("myftp> ");
		cin.getline(buf, BUFFER);

		//deal with empty input
		if (buf[0] == NULL) {
			continue;
		}
		//deal with whitespace
		if (isspace(buf[0])) {
			continue;
		}
		
		//tokenize for STRCMP for GET and PUT, don't want a false positive from STRSTR
		//	also allows more detailed error messages regarding specific files
		
		char *moby=(char *) malloc(BUFFER);
		char *dick=(char *) malloc(BUFFER);
		bool extraArgs=false;
		bool amperSand=false;
		strcpy(moby, buf);
		moby = strtok (moby," ");
		if (moby != NULL){
			dick = strtok (NULL, " \n");
		}
		if (strtok(NULL, " \n")!=NULL){
			extraArgs=true;
		}
		//check for ampersand, assuming no funny business as per PointsToNote(3) on proj specs
		for(int bing = 0; bing < strlen(buf); bing++) {
			if (buf[bing] == '&') {
				amperSand = true;
			}
		}
		
		// RECV/SEND for command: GET
		if (strcmp(moby, "get") == 0) {
			if (amperSand) {
				char whale[BUFFER];
			
				//send get <filename> <&> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				
				//recv command ID and print to screen so client knows it
				if (recv(sock, whale, sizeof(whale), 0) < 0) {
					perror("ERROR: Problems receiving Command ID from server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Command ID is: %s\n", whale);
				
				//proceed with GET as normal
				int sizeofile = 0;
				char msg[BUFFER];
				FILE *file = fopen(dick, "w");

				while(sizeofile = recv(sock, msg, BUFFER, 0)) {
					//file does not exist
					if ((strcmp(msg, eof)) == 0) {
						printf("File does not exist in client:%d's directory\n", sock);
						//delete empty file
						remove(dick);
						break;
					}

					//file exists
					fwrite(msg, sizeof(char), sizeofile, file);
					if (sizeofile <= BUFFER) {
						//client is done receiving
						break;
					}
				}
				fclose(file);
			}
			else if(extraArgs){
				printf("GET error: must have exactly one argument\n");
				printf("GET error: or must have exactly two arguments: <argument> <&>\n");
			} //if (extraArgs)
			else{
				//send get <filename> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				} //if (i<0)

				//recv File status
				memset(buf, '\0', BUFFER);
				if (recv(sock, buf, BUFFER, 0) < 0){
					perror("ERROR: Failed to receive file status from server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				} //if (recv(sock, buf, BUFFER, 0) < 0)
				
				//file does not exist, print to user, loop back to prompt
				if (strstr(buf, "NULL")) {
					printf("Sorry, the file, %s, you requested DOES NOT EXIST in the REMOTE directory.\n", dick);
					printf("Cannot perform GET <%s>\n", dick);
				} //if (strstr(buf, "NULL"))
			
				//file does exist, create data socket/bind/listen/accept, send connection status
				else {
					size_t size;
					char data[BUFFER];
					memset(data, 0, BUFFER);
					//overwrite existing file or create new file
					FILE* doc = fopen(buf, "wb");
					
					char *openfile;
					//cannot open file to write
					if (doc == NULL) {
						openfile = "CANT";
						//send file status, end
						if (send(sock, buf, sizeof(buf), 0) < 0) {
							perror("ERROR: Cannot send file opening status to server\n");
							close(sock);
							exit(EXIT_FAILURE);
						} //if (send(sock, buf, sizeof(buf), 0) < 0)
						perror("ERROR: Cannot open file to be written.\n");
						close(sock);
						exit(EXIT_FAILURE);
					} //if (doc == NULL)
					//we are ready to receive
					else {
						openfile = "GOOD";
						//send GOOD TO GO, let's start sending that file
						if (send(sock, buf, sizeof(buf), 0) < 0) {
							perror("ERROR: Cannot send ready to receive clearance to server.\n");
							fclose(doc);
							close(sock);
							exit(EXIT_FAILURE);
						} //if (send(sock, buf, sizeof(buf), 0) < 0)
					
						while ((size = recv(sock, data, sizeof(data), 0)) > 0) {
							//if EOF, break
							if ((strcmp(data, eof)) == 0) {
								break;
							} //if ((strcmp(data, eof)) == 0)
							fwrite(data, 1, BUFFER, doc);
						} //while ((size = recv(sock, data, sizeof(data), 0)) > 0)
						if (size < 0) {
							perror("ERROR: Problems receiving file from server.\n");
							fclose(doc);
							close(sock);
							exit(EXIT_FAILURE);
						} //if (size < 0)
						//printf("closing file\n"); //tk
						fclose(doc);
					} //else
				} //else
			} //else

		} //if (strstr(buf, "get"))
		else if (strcmp(moby, "put") == 0) {
			if (amperSand) {
				char whale[BUFFER];

				//send put <filename> <&> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}

				//recv command ID and print to screen so client knows it
				if (recv(sock, whale, sizeof(whale), 0) < 0) {
					perror("ERROR: Problems receiving Command ID from server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				printf("Command ID is: %s\n", whale);
				
				//proceed with PUT as normal
				int sizeofile = 0;
				char msg[BUFFER];
				FILE *file = fopen(dick, "r");
				
				if (file == NULL) {
					printf("%s does not exist in local directory\n", dick);
					send(sock, eof, sizeof(eof), 0);
				}
				else {
					while(sizeofile = fread(msg, sizeof(char), BUFFER, file)) {
						send(sock, msg, sizeofile, 0);
						memset(msg, '\0', BUFFER);
						if (sizeofile == 0) {
							break;
						}
					}
					fclose(file);
				}
			}
			else if(extraArgs){
				printf("PUT error: must have exactly one argument\n");
				printf("PUT error: or must have exactly two arguments: <argument> <&>\n");
			} //if (extraArgs)
			else{
				//send put <filename> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				} //if (i<0)
			
				put_file(dick, sock);
			} //else

		}
		// WRITE/READ for commands: LS, PWD
		else {
			i=write(sock, buf, strlen(buf));
			if (i<0){
				perror("write");
				exit(4);
			} //if (i<0)

			i=read(sock, buf, BUFFER);
			if (i<0){
				perror("read");
				exit(5);
			} //if (i<0)
		} //else
		printf("%s\n", buf);
	} //while(strcmp(buf, "exit")!=0)
	
	close(sock);

	exit(0);

}
