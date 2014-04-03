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
#include <map>
#include <fstream>
#include <pthread.h>

using namespace std;

#define BUFFER 1024
#define THREAD 100

const char eof[] = "EOF";

typedef map<char*, bool> terminator;
terminator arnold;

//ampersand thread data
struct data_thread {
	int sockid;
	char* nameofile;
	char* host;
	char* path;
};

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

//make connection to server given hostname and port number
int make_connection(const char *host, const char* port) {
	int sock;
	struct addrinfo hint, *res, *j;
	
	//obtain address(es) matching hostname and port number
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_family = AF_INET;			//IPv4
	hint.ai_socktype = SOCK_STREAM;	//TCP socket
	
	//getaddrinfo(hostname, port number, address specifications, start of linked list of address structs)
	if (getaddrinfo(host, port, &hint, &res) < 0) {
		perror("ERROR: Cannot resolve the address.\n");
		exit(EXIT_FAILURE);
	}
	
	//try each address until successful connection
	for (j = res; j != NULL; j = j->ai_next) {
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
	
	freeaddrinfo(res);
	
	return sock;
}

void *get (void *threadinfo){
	struct data_thread *dt = (struct data_thread*) threadinfo;
	int sock = dt->sockid;
	char* dick = dt->nameofile;
	char* host = dt->host;
	char* path = dt->path;

	char whale[BUFFER];
	memset(whale, '\0', sizeof(whale));
	
	//recv port number for &GET data connection
	if (recv(sock, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems receiving Command ID from server.\n");
		close(sock);
		exit(EXIT_FAILURE);
	}
	printf("Data Connection Port is: %s\n", whale);
	
	//connect to data connection
	int tempo;
	tempo = make_connection(host, whale);
	memset(whale, '\0', sizeof(whale));
	
	//recv command ID and print to screen so client knows it
	if (recv(tempo, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems receiving Command ID from server.\n");
		close(tempo);
		exit(EXIT_FAILURE);
	}
	printf("Command ID is: %s\n", whale);
	
	//insert into client terminate resolution map
	arnold.insert(pair<char*, bool>(whale, false));
	
	//check file existence for cleanup later
	ifstream ifile(dick);
	bool existence = false;
	if (ifile) {
		existence = true;
	}
	ifile.close();
	
	//proceed with GET as normal
	int sizeofile = 0;
	char msg[BUFFER];
	FILE *file = fopen(dick, "w");
	bool breakout = false;

	while(sizeofile = recv(tempo, msg, BUFFER, 0)) {
		//file does not exist
		if ((strcmp(msg, eof)) == 0) {
			printf("File does not exist in client:%d's directory\n", tempo);
			//delete empty file
			remove(dick);
			break;
		}

		//file exists
		fwrite(msg, sizeof(char), sizeofile, file);
		
		//check terminate status
		for(terminator::iterator man = arnold.begin();  man != arnold.end(); man++) {
			if (strcmp(man->first, whale) == 0) {
				if (man->second == true) {
					breakout = true;
				}
			}
		}
		if (breakout) {
			printf("Terminating on client-side &GET\n\n");
			//if overwrite existing file: keep, else delete new file
			if (!existence) {
				remove(path);
			}
			break;
		}
	}
	close(tempo);
	fclose(file);
	//delete commandID from map
	for(terminator::iterator rich = arnold.begin();  rich != arnold.end(); rich++) {
		if (strcmp(rich->first, whale) == 0) {
			arnold.erase(rich++);
		}
	}
	
	//wait for thread to finish and then terminate it
	pthread_exit(NULL);
}

void *put (void *threadinfo){
	struct data_thread *dt = (struct data_thread*) threadinfo;
	int sock = dt->sockid;
	char* dick = dt->nameofile;
	char* host = dt->host;
	char* path = dt->path;
	
	char whale[BUFFER];
	memset(whale, '\0', sizeof(whale));
	
	//recv port number for &GET data connection
	if (recv(sock, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems receiving Command ID from server.\n");
		close(sock);
		exit(EXIT_FAILURE);
	}
	printf("Data Connection Port is: %s\n", whale);
	
	//connect to data connection
	int tempo;
	tempo = make_connection(host, whale);
	memset(whale, '\0', sizeof(whale));

	//recv command ID and print to screen so client knows it
	if (recv(tempo, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems receiving Command ID from server.\n");
		close(tempo);
		exit(EXIT_FAILURE);
	}
	printf("Command ID is: %s\n", whale);
	
	//insert into client terminate resolution map
	arnold.insert(pair<char*, bool>(whale, false));
	
	//proceed with PUT as normal
	int sizeofile = 0;
	char msg[BUFFER];
	FILE *file = fopen(dick, "r");
	bool breakout = false;
	
	if (file == NULL) {
		printf("%s does not exist in local directory\n", dick);
		send(tempo, eof, sizeof(eof), 0);
	}
	else {
		while(sizeofile = fread(msg, sizeof(char), BUFFER, file)) {
			send(tempo, msg, sizeofile, 0);
			memset(msg, '\0', BUFFER);
			
			//check terminate status
			for(terminator::iterator man = arnold.begin();  man != arnold.end(); man++) {
				if (strcmp(man->first, whale) == 0) {
					if (man->second == true) {
						breakout = true;
					}
				}
			}
			if (breakout) {
				printf("Terminating on server-side &PUT\n\n");
				break;
			}
		}
		close(tempo);
		fclose(file);
		//delete commandID from map
		for(terminator::iterator rich = arnold.begin();  rich != arnold.end(); rich++) {
			if (strcmp(rich->first, whale) == 0) {
				arnold.erase(rich++);
			}
		}
	}
	
	//wait for thread to finish and then terminate it
	pthread_exit(dt);
}

void terminate(char* arg) {
	/*char* whale = "fuck";
	char* bhale = "your";
	char* nnale = "shitt";

	arnold.insert(pair<char*, bool>(whale, false));
	arnold.insert(pair<char*, bool>(bhale, false));
	arnold.insert(pair<char*, bool>(nnale, false));*/
	
	bool fool = false;
	
	for(terminator::iterator i = arnold.begin();  i != arnold.end(); i++) {
		if (strcmp(i->first, arg) == 0) {
			i->second = true;
			printf("Termination Status Flipped On\n");
			fool = true;
		}
		printf("%s == %d\n", i->first, i->second);
	}
	if(!fool) {
		printf("Command ID invalid or function completed before termination\n");
	}
}

int main(int argc, char *argv[]){
	int sock, tsock, i, tempo;
	char buf[BUFFER];

	//check for proper command args
	if (argc != 4){
		printf("Usage: %s <server hostname> <nport number> <tport number>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	//check that N-Port and T-Port are not the same
	if (argv[2] == argv[3]) {
		printf("N-Port: %s and T-Port: %s cannot be equal", argv[2], argv[3]);
		exit(EXIT_FAILURE);
	}

	sock = make_connection(argv[1], argv[2]);
	tsock = make_connection(argv[1], argv[3]);
	printf("sock: %d\ntsock: %d\n", sock, tsock);	//tk
	
	char homeDir[BUFFER];
	//initialize home directory
	if (getcwd(homeDir, sizeof(homeDir)) == NULL){
		perror("Home Directory init error");
	}
	
	int threadMAX = 0;
	
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
		
		//dont want to access a thread out of bounds
		if (threadMAX == THREAD) {
			threadMAX = 0;
		}
		
		// RECV/SEND for command: GET
		if (strcmp(moby, "get") == 0) {
			if (amperSand) {
				//send get <filename> <&> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				
				printf("threadMAX: %d\n", threadMAX);	//tk
				threadMAX = threadMAX + 1;
			
				char path[1024];
				strcpy(path, homeDir);
				strcat(path, "/");
				strcat(path, dick);
			
				//create <GET &> thread, thread data struct
				pthread_t thread_ID[THREAD];
				struct data_thread dt[THREAD];

				//initialize <GET &> thread data
				dt[threadMAX].sockid = sock;
				dt[threadMAX].nameofile = dick;
				dt[threadMAX].host = argv[1];
				dt[threadMAX].path = path;
				
				//initialize and start <GET &> thread
				pthread_create(&thread_ID[threadMAX], NULL, get, (void *) &dt[threadMAX]);
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
				//send put <filename> <&> to server
				i=write(sock, buf, strlen(buf));
				if (i<0){
					perror("ERROR: Failed to write command to server.\n");
					close(sock);
					exit(EXIT_FAILURE);
				}
				
				char path[1024];
				strcpy(path, homeDir);
				strcat(path, "/");
				strcat(path, dick);
			
				//create <PUT &> thread, thread data struct
				pthread_t thread_ID;
				struct data_thread *dt = (data_thread*)malloc(sizeof(data_thread));

				//initialize <PUT &> thread data
				dt->sockid = sock;
				dt->nameofile = dick;
				dt->host = argv[1];
				dt->path = path;
				
				//initialize and start <PUT &> thread
				pthread_create(&thread_ID, NULL, put, (void *)dt);
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
		else if (strcmp(moby, "terminate") == 0) {
			if(extraArgs){
				printf("TERMINATE error: must have exactly one argument\n");
			} //if (extraArgs)
			else{
				//Send termination order via T-Port
				i = write(sock, buf, strlen(buf));
				if (i < 0){
					perror("ERROR: Failed to write termination to server.\n");
					close(tsock);
					close(sock);
					exit(EXIT_FAILURE);
				}
				
				terminate(dick);
			}
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
	close(tsock);

	exit(0);

}
