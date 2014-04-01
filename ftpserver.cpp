#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>		//needed for creating Internet addresses
#include <unordered_map> 	//needed for hashmap
#include <sstream>

#define BUFFER 1024

const char eof[] = "EOF";

struct thread_data{
	int sid;
};

//ampersand thread data
struct data_thread {
	int sockid;
	char* nameofile;
	char* pathname;
};

struct gate_keeper {
	int readerCount;
	char writeTest[1024];
	pthread_mutex_t readMutex;
	pthread_mutex_t dataMutex;
	gate_keeper(){
		readerCount=0;
		pthread_mutex_init(&(readMutex), NULL);
		pthread_mutex_init(&(dataMutex), NULL);
		strcpy(writeTest, "untouched");
	}
};

char homeDir[BUFFER];
std::unordered_map<std::string, gate_keeper> fileLocks;

void lock_reader(gate_keeper *mutexGuard){
	pthread_mutex_lock(&(mutexGuard->readMutex));
	++(mutexGuard->readerCount);
	if(mutexGuard->readerCount==1){
		pthread_mutex_lock(&(mutexGuard->dataMutex));
	}
	pthread_mutex_unlock(&(mutexGuard->readMutex));
}

void unlock_reader(gate_keeper *mutexGuard){
	pthread_mutex_lock(&(mutexGuard->readMutex));
	--(mutexGuard->readerCount);
	if(mutexGuard->readerCount==0){
		pthread_mutex_unlock(&(mutexGuard->dataMutex));
	}
	pthread_mutex_unlock(&(mutexGuard->readMutex));
}

void lock_writer(gate_keeper *mutexGuard){
	pthread_mutex_lock(&(mutexGuard->dataMutex));
}

void unlock_writer(gate_keeper *mutexGuard){
	pthread_mutex_unlock(&(mutexGuard->dataMutex));
}

void get_file(char* filename, char* cwd, int sockid){
	//open file and send file status to client
	char *status = "NULL";
	char path[1024];
	strcpy(path, cwd);
	strcat(path, "/");
	strcat(path, filename);
	
	//check if file is in table; get locks for reader
	if(fileLocks.find(path) == fileLocks.end()){
		gate_keeper zuul;
		fileLocks[path]=zuul;
	}			
	lock_reader(&fileLocks[path]);
	
	FILE* doc = fopen(path, "rb");
	
	//file DOES NOT exist, send file status, end function
	if (doc == NULL) {
		printf("%s does NOT exist\n", filename);
		if (send(sockid, status, (int)strlen(status), 0) < 0){
			perror("ERROR: Failed to send file status to client.\n");
			close(sockid);
			unlock_reader(&fileLocks[path]);
			exit(EXIT_FAILURE);
		} //if (send(sockid, status, (int)strlen(status), 0) < 0)
		return;
	} //if (doc == NULL)
	//file EXISTS, send file status
	else{
		if (send(sockid, filename, (int)strlen(filename), 0) < 0){
			perror("ERROR: Failed to send file status to client");
			fclose(doc);
			close(sockid);
			unlock_reader(&fileLocks[path]);
			exit(EXIT_FAILURE);
		} //if (send(sockid, filename, (int)strlen(filename), 0) < 0)
		
		char data[BUFFER];
		//failure to receive client file opening status
		if (recv(sockid, data, sizeof(data), 0) < 0){
			perror("ERROR: Failed to receive file opening status from server.\n");
			fclose(doc);
			close(sockid);
			unlock_reader(&fileLocks[path]);
			exit(EXIT_FAILURE);
		} //if (recv(sockid, data, sizeof(data), 0) < 0)
		//client sent failure to open file
		if (strstr(data, "CANT")) {
			perror("ERROR: Client was not able to open file to be ready for receiving.\n");
			fclose(doc);
			close(sockid);
			unlock_reader(&fileLocks[path]);
			exit(EXIT_FAILURE);
		} //if (strstr(data, "CANT"))
		else{
			size_t size;
			memset(data, '\0', BUFFER);
			
			do {
				size = fread(data, 1, sizeof(data), doc);
				
				if (size < 0) {
					perror("ERROR: Cannot fread() file.\n");
					fclose(doc);
					close(sockid);
					unlock_reader(&fileLocks[path]);
					exit(EXIT_FAILURE);
				}
				
				//send
				if (send(sockid, data, size, 0) < 0) {
					perror("ERROR: Cannot send file.\n");
					fclose(doc);
					close(sockid);
					unlock_reader(&fileLocks[path]);
					exit(EXIT_FAILURE);
				}
			} while (size > 0);
			
			//send end of file
			if (send(sockid, eof, sizeof(eof), 0) < 0) {
				perror("ERROR: Cannot send 'end of file' signal to client.\n");
				fclose(doc);
				close(sockid);
				unlock_reader(&fileLocks[path]);
				exit(EXIT_FAILURE);
			}
			
			//close file
			fclose(doc);
			unlock_reader(&fileLocks[path]);
		} //else
	} //else
} //void get_file(char* filename, int sockid)

void put_file(char* filename, char* cwd, int sockid){
	char buf[BUFFER];
	memset(buf, '\0', BUFFER);
	
	//recv File status = exist || does not exist
	if (recv(sockid, buf, BUFFER, 0) < 0) {
		perror("ERROR: Failed to receive file status from client.\n");
		close(sockid);
		exit(EXIT_FAILURE);
	}
	
	//file DOES NOT exist, print to user, end function, back to parsing more commands
	if (strstr(buf, "NULL")) {
		printf("The file DOES NOT EXIST in the client's directory.\n");
		return;
	}
	//file EXISTs...
	else {
		size_t size;
		char data[BUFFER];
		memset(data, '\0', BUFFER);

		char path[1024];
		strcpy(path, cwd);
		strcat(path, "/");
		strcat(path, filename);
		
		//checks if file already exists in hash; locks for writer
		if(fileLocks.find(path) == fileLocks.end()){
			gate_keeper zuul;
			fileLocks[path]=zuul;
		}
		lock_writer(&fileLocks[path]);
		
		//will open no matter what, file is open for writing/receiving
		FILE* doc = fopen(path, "wb");
		
		//send GOOD TO GO, let's start sending that file client
		//	this has more to do with BLOCKing the client so that the server can start
		//	receiving before the client starts sending the file
		if (send(sockid, buf, sizeof(buf), 0) < 0) {
			perror("ERROR: Cannot send ready to receive clearance to client.\n");
			fclose(doc);
			close(sockid);
			unlock_writer(&fileLocks[path]);
			exit(EXIT_FAILURE);
		}
		
		//start receiving file
		while ((size = recv(sockid, data, sizeof(data), 0)) > 0) {
			printf("receiving...\n");
			//if EOF, break
			if ((strcmp(data, eof)) == 0) {
				printf("received EOF\n");
				break;
			}
			fwrite(data, 1, BUFFER, doc);
		}
		
		//recv error
		if (size < 0) {
			perror("ERROR: Problems receiving file from client.\n");
			fclose(doc);
			close(sockid);
			unlock_writer(&fileLocks[path]);
			exit(EXIT_FAILURE);
		}
		
		fclose(doc);
		printf("%s received from client local directory to server remote directory.\n", filename);
		unlock_writer(&fileLocks[path]);
	}
}

//threaded get
void *get (void *threadinfo){
	struct data_thread *dt = (struct data_thread*) threadinfo;
	int sockid = dt->sockid;
	char* filename = dt->nameofile;
	char* path = dt->pathname;
	
//	printf("filename: %s & sockid: %d\n", filename, sockid);
//	printf("thread is %u\n", pthread_self());
//	printf("pathname: %s\n", path);
	
	std::ostringstream oss;
	unsigned int yarn = pthread_self();
	oss << yarn;
	char whale[BUFFER];
	strcpy(whale, oss.str().c_str());
	
	//send command ID
	if (send(sockid, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems sending Command ID\n");
		pthread_exit(NULL);
	}
	
	//check if file is in table; get locks for reader
	if(fileLocks.find(path) == fileLocks.end()){
		gate_keeper zuul;
		fileLocks[path]=zuul;
	}			
	lock_reader(&fileLocks[path]);
	
	//proceed with GET as normal
	int sizeofile = 0;
	char msg[BUFFER];
	FILE *file = fopen(path, "r");
	
	if (file == NULL) {
		printf("%s does not exist in remote/server's directory\n", filename);
		send(sockid, eof, sizeof(eof), 0);
		unlock_reader(&fileLocks[path]);
	}
	else {
		while(sizeofile = fread(msg, sizeof(char), BUFFER, file)) {
			send(sockid, msg, sizeofile, 0);
			memset(msg, '\0', BUFFER);
			if (sizeofile == 0) {
				break;
			}
		}
		fclose(file);
		unlock_reader(&fileLocks[path]);
	}
}

//threaded put
void *put (void *threadinfo) {
	struct data_thread *dt = (struct data_thread*) threadinfo;
	int sockid = dt->sockid;
	char* filename = dt->nameofile;
	char* path = dt->pathname;
	
//	printf("filename: %s & sockid: %d\n", filename, sockid);
//	printf("thread is %u\n", pthread_self());
//	printf("pathname: %s\n", path);

	std::ostringstream oss;
	unsigned int yarn = pthread_self();
	oss << yarn;
	char whale[BUFFER];
	strcpy(whale, oss.str().c_str());
	
	//send command ID
	if (send(sockid, whale, sizeof(whale), 0) < 0) {
		perror("ERROR: Problems sending Command ID\n");
		pthread_exit(NULL);
	}
	
	//proceed with PUT as normal
	int sizeofile = 0;
	char msg[BUFFER];
	
	//checks if file already exists in hash; locks for writer
	if(fileLocks.find(path) == fileLocks.end()){
		gate_keeper zuul;
		fileLocks[path]=zuul;
	}
	lock_writer(&fileLocks[path]);
	
	FILE *file = fopen(path, "w");
	
	while(sizeofile = recv(sockid, msg, BUFFER, 0)) {
		//file does not exist
		if ((strcmp(msg, eof)) == 0) {
			printf("File does not exist in client:%d's directory\n", sockid);
			//delete empty file
			remove(path);
			break;
		}

		//file exists
		fwrite(msg, sizeof(char), sizeofile, file);
		if (sizeofile <= BUFFER) {
			//server is done receiving
			break;
		}
	}
	fclose(file);
	unlock_writer(&fileLocks[path]);
}

//Prints the message from the clients and writes the same message back to the client
void *Echo (void *threadargs){
	int wCheck;
	struct thread_data *data;
	data=(struct thread_data *) threadargs;
	int sid=data->sid;
	char str[BUFFER];
	char cwd[BUFFER];
	strcpy(cwd, homeDir);
	
	while(strcmp(str, "quit")!=0){
		memset(str, '\0', BUFFER);
		if (read(sid, str, BUFFER)<0){
			perror("read");
			exit(-4);
		}
		
		//Tokenize the client's request
		char *command=(char *) malloc(BUFFER);
		char *cargs=(char *) malloc(BUFFER);
		bool extraArgs=false;
		bool amperSand=false;
		strcpy(command, str);
		command = strtok (command," ");
		if (command != NULL){
			cargs = strtok (NULL, " \n");
		}
		if (strtok(NULL, " \n")!=NULL){
			extraArgs=true;
		}
		//check for ampersand, assuming no funny business as per PointsToNote(3) on proj specs
		for(int bing = 0; bing < strlen(str); bing++) {
			if (str[bing] == '&') {
				amperSand = true;
			}
		}
		
		
		//Switch for the 7 main commands (not including exit). Else is echo
		if(strcmp(command, "get")==0) {
			if (!amperSand) {
				get_file(cargs, cwd, sid);
			}
			else {
				char path[1024];
				strcpy(path, cwd);
				strcat(path, "/");
				strcat(path, cargs);
			
				//create <GET &> thread, thread data struct
				pthread_t thread_ID;
				struct data_thread *dt = (data_thread*)malloc(sizeof(data_thread));

				//initialize <GET &> thread data
				dt->sockid = sid;
				dt->nameofile = cargs;
				dt->pathname = path;

				//initialize and start <PUT &> thread
				pthread_create(&thread_ID, NULL, get, (void *)dt);
				
				//wait for thread to finish and then terminate it
				(void) pthread_join(thread_ID, NULL);
			}
		} //get <filename> request
		else if (strcmp(command, "put") == 0) {
			if (!amperSand) {
				put_file(cargs, cwd, sid);
			}
			else {
				char path[1024];
				strcpy(path, cwd);
				strcat(path, "/");
				strcat(path, cargs);
				
				//create <PUT &> thread, thread data struct
				pthread_t thread_ID;
				struct data_thread *dt = (data_thread*)malloc(sizeof(data_thread));

				//initialize <PUT &> thread data
				dt->sockid = sid;
				dt->nameofile = cargs;
				dt->pathname = path;

				//initialize and start <PUT &> thread
				pthread_create(&thread_ID, NULL, put, (void *)dt);
				
				//wait for thread to finish and then terminate it
				(void) pthread_join(thread_ID, NULL);
			}
		} //put <filename> request
		else {
			if(strcmp(command, "delete")==0){
				if (cargs==NULL || extraArgs){
					printf("DELETE: Invalid number of arguments\n");
					strcpy(str, "DELETE error: must have exactly one argument\n");
				}
				else{
					char path[1024];
					strcpy(path, cwd);
					strcat(path, "/");
					strcat(path, cargs);
					if (remove(path)!=0){
						perror("Error deleting file");
						strcpy(str, "Couldn't delete the file: ");
						strcat(str, cargs);
					}
					else{
						strcpy(str, cargs);
						strcat(str, " successfully deleted!\n");
					}
				}
				
			} //delete <filename> request
			else if( strcmp(command, "ls")==0 ){
				if (cargs!=NULL){
					printf("LS: command must not have arguments\n");
					strcpy(str, "LS error: must have no arguments");
				}
				else{
					DIR *dir;
					struct dirent *entry;
					char lsContents[BUFFER]="\n";
					if ((dir = opendir(cwd)) == NULL){
						perror("Opening the directory");
						strcpy(str, "Failed to open directory object\n");
					}
					else {
						while ((entry = readdir(dir)) != NULL){
							//Check each first char to skip '.' and '..'
							if ((entry->d_name)[0] != '.') {
								printf("  %s\n", entry->d_name);
								strcat(lsContents, entry->d_name);
								strcat(lsContents, "\n");
							}
						}
						printf("\n");
						closedir(dir);
						strcpy(str, lsContents);
					}
				}
				
			} //ls request
			else if(strcmp(command, "cd")==0){
				char path[1024];
				strcpy(path, cwd);
				strcat(path, "/");
				strcat(path, cargs);
				if (cargs==NULL || extraArgs){
					printf("CD: Invalid number of arguments\n");
					strcpy(str, "CD error: must have exactly one argument\n");
				}
				else if (chdir(path)!=0){
					perror("chdir() error");
					strcpy(str, "Failed to change directory\n");
				}
				else{
					getcwd(cwd, sizeof(cwd));
					strcpy(str, "Successfully changed the working directory!\n");
				}
				
			} //cd request
			else if(strcmp(command, "mkdir")==0 ){
				char path[1024];
				strcpy(path, cwd);
				strcat(path, "/");
				strcat(path, cargs);
				if (cargs==NULL || extraArgs){
					printf("MKDIR: Invalid number of arguments\n");
					strcpy(str, "MKDIR error: must have exactly one argument\n");
				}
				else{
					if (mkdir(path, S_IRWXU|S_IRGRP|S_IXGRP) != 0){
						perror("mkdir() error");
						strcpy(str, "Failed to create the directory: ");
						strcat(str, cargs);
					}
					else{
						strcpy(str, cargs);
						strcat(str, " directory successfully created\n");
					}
				}
				
			} //mkdir <directory name> request
			else if( strcmp(command, "pwd")==0 ){
				if (cargs!=NULL){
					printf("PWD: command must have no arguments\n");
					strcpy(str, "PWD error: must have no arguments\n");
				}
				else{
					if (strcpy(str, cwd) == NULL){
						perror("pwd error");
						strcpy(str, "pwd failed\n");
					}
					else{
						printf("CWD is: %s\n", str);
					}
				}	
			} //pwd request
			
			else if ( strcmp(command, "quit")==0 ){
				if (cargs!=NULL){
					printf("QUIT: command must have no arguments\n");
					strcpy(str, "QUIT error: must have no arguments\n");
				}
				else{
					printf("Client has quit\n");
				}
				
			} //quit request
			/*
			else if( strcmp(command, "read")==0 ){
				printf("Thread # %d trying to get read lock\n", pthread_self());
				
				if(fileLocks.find(cargs) == fileLocks.end()){
					gate_keeper test;
					fileLocks[cargs]=test;
				}
				
				lock_reader(&fileLocks[cargs]);
				printf("Thread # %d read %d\n", pthread_self(), fileLocks[cargs].readerCount);
				printf("writeTest is %s\n", fileLocks[cargs].writeTest);
				sleep(30);
				unlock_reader(&fileLocks[cargs]);
				printf("Thread # %d released the read lock\n", pthread_self());
			}
			
			else if( strcmp(command, "write")==0 ){
				printf("Thread # %d trying to get write lock\n", pthread_self());
				
				if(fileLocks.find(cargs) == fileLocks.end()){
					gate_keeper test;
					fileLocks[cargs]=test;
				}
				
				lock_writer(&fileLocks[cargs]);
				strcpy(fileLocks[cargs].writeTest, "1");
				printf("Thread # %d wrote %s\n", pthread_self(), fileLocks[cargs].writeTest);
				sleep(30);
				unlock_writer(&fileLocks[cargs]);
				printf("Thread # %d released the write lock\n", pthread_self());
			}
			*/
			
			else{
				printf("Unrecognised command: %s\n", str);
				strcat(str, " command is unrecognized.\n");
			} //unrecognized request
			
			//write back to the client
			wCheck=write(sid, str, BUFFER);
			if (wCheck<0){
				perror("write\n");
				exit(-7);
			} //if (wCheck<0)
		} //else	
	} //while(strcmp(str, "exit")!=0)
	
	if (chdir(homeDir)!=0){
		perror("Couldn't return to home directory upon closing client");
	}
	
	close(sid);

	return NULL;
}

int main(int argc, char *argv[]){

	//kills zombie children
	signal(SIGCHLD, SIG_IGN);

	struct addrinfo hints, *results;
	int sockid, client, reuse;
	int numThreads=2;
	pthread_t threads[numThreads];
	struct thread_data data_arr[numThreads];
	
	//checks for command-line params
	if(argc!=2){
		printf("Usage: %s <port number>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;				//IPv4
	hints.ai_socktype = SOCK_STREAM;		//TCP
	hints.ai_flags = AI_PASSIVE;			//use current IP
	
	if (getaddrinfo(NULL, argv[1], &hints, &results) < 0) {
		perror("Cannot resolve the address\n");
		exit(EXIT_FAILURE);
	}
	
	//create socket file descriptor
	sockid = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
	
	//check for socket creation failure
	if (sockid < 0) {
		perror("Cannot open socket\n");
		exit(EXIT_FAILURE);
	}
	
	printf("got a socket number: %d\n", sockid);
	
	//port reuse
	reuse = 1;
	if (setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int)) < 0) {
		perror("Cannot set reuse option for socket\n");
		exit(EXIT_FAILURE);
	}
	
	//bind socket to given port
	if (bind(sockid, results->ai_addr, results->ai_addrlen) < 0) {
		perror("Cannot bind socket to given port\n");
		close(sockid);
		exit(EXIT_FAILURE);
	}
	
	//initialize home directory
	if (getcwd(homeDir, sizeof(homeDir)) == NULL){
		perror("Home Directory init error");
	}
	
	//listen (queue of 5) for incoming connections
	if (listen(sockid, 5) < 0) {
		perror("Cannot listen to socket\n");
		close(sockid);
		exit(EXIT_FAILURE);
	}
	
	//Accepts a client and calls the echo function
	int m;
	//store details about the client who has connected
	struct sockaddr_in saddr;
	unsigned int addrlen = sizeof(saddr);
	while (1){
		//accept and create a separate socket for client(s)
		//printf("trying to connect\n");
		if ((client=accept(sockid, (struct sockaddr *) &saddr, &addrlen)) < 0) {
			perror("Cannot open client socket\n");
			exit(EXIT_FAILURE);
		}
		//printf("connected\n");
		
		data_arr[0].sid=client;
		m = pthread_create(&threads[0], NULL, Echo, (void *) &data_arr[0]);
		if (m){
			perror("Pthread");
			exit(-5);
		}
		//printf("Thread quit!\n");
	}
	pthread_exit(NULL);
	exit(0);
}
