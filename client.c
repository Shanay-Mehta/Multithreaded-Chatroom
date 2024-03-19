#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> 

char username[1024];
int socket_id;
int list_checker=0;
void *usr_inp(void *param);
void *ser_rec(void *param);
// Create a connection to the server
int create_connection(char* addr, int port) {
	int client_sockfd;
	if ((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		exit(1);
	}
	struct sockaddr_in server_addrinfo;
	server_addrinfo.sin_family = AF_INET;
	server_addrinfo.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0)
	{
		close(client_sockfd);
		exit(1);
	}
	if (connect(client_sockfd, (struct sockaddr *)&server_addrinfo, sizeof(server_addrinfo)) == -1)
	{
		printf("Could not find server");
        fflush(stdout);
		close(client_sockfd);
		exit(1);
	}
    if (send(client_sockfd, username, (int)strlen(username), 0) == -1){
        close(client_sockfd);
        exit(1);
    }
	return client_sockfd;
}
int main(int argc, char *argv[])
{
    if (argc != 3)
	{
		printf("Use 2 cli arguments\n");
        fflush(stdout);
		return -1;
	}
    
	// extract the address and port from the command line arguments
	char serverIP[INET6_ADDRSTRLEN];
	unsigned int server_port;
	strcpy(serverIP, argv[1]);
	server_port = atoi(argv[2]);
    int count=0;
    memset(username,0,sizeof(username));
    fgets(username,1024,stdin); //err: blocking call
    username[strlen(username)-1]='\0';
	socket_id = create_connection(serverIP, server_port);
    pthread_t tid1,tid2;
	pthread_create(&tid1,NULL,usr_inp,NULL);
    pthread_create(&tid2,NULL,ser_rec,NULL);
	tid1=pthread_join(tid1,NULL);
    tid2=pthread_join(tid2,NULL);
    return 0;
}
void *usr_inp (void *param)
{ 	
    while(1){
        char msg[1024];
        memset(msg,0,sizeof(msg));
        fgets(msg,1024,stdin); //blocking call
        if (send(socket_id, msg, (int)strlen(msg), 0) == -1){
            close(socket_id);
            exit(1);
        }
        if(strncmp(msg,"EXIT\n",5)==0){ //err: Print any message+exit(1) exits all?
            close(socket_id);
		    exit(1);
        }
    }
	pthread_exit(0);
}
void *ser_rec ( void *param )
{ 	
    while(1){
        char reply[1024];
        memset(reply,0,sizeof(reply));
        if (recv(socket_id, reply, 1024, 0) == -1){
            exit(1);
        }
        char* token=strtok(reply,"\n");
        while(token!=NULL){
            printf("%s\n",token);
            fflush(stdout);
            token=strtok(NULL,"\n");
        }
    }
	pthread_exit(0);
}