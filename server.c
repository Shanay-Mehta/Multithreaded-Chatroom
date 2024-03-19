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
#include <semaphore.h>

// semaphores for grps and grp_cnt handling
sem_t grps_sem,his_sem;

typedef struct clients{
	int sockid;
	char username[1024];
	int status;
    int my_grps[1024];
    int tot_grps;
}clients;

clients arr[1024];

char history[10000];

char grps[1024][1024];

int grp_cnt;

// FILE* fp;

int socket_id;

void *make_conn(void *param);

void *cli_rec(void *param);

// create connction
int create_connection(char* addr, int port) {
	int server_sockfd;
	int yes = 1;
	if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		exit(1);
	}
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
	{
		close(server_sockfd);
		exit(1);
	}
	struct sockaddr_in server_addrinfo;
	server_addrinfo.sin_family = AF_INET;
	server_addrinfo.sin_port = htons(port);
	// server_addrinfo.sin_addr.s_addr = htonl(INADDR_ANY);
	if (inet_pton(AF_INET, addr, &server_addrinfo.sin_addr) <= 0)
	{
		close(server_sockfd);
		exit(1);
	}
	if (bind(server_sockfd, (struct sockaddr *)&server_addrinfo, sizeof(server_addrinfo)) == -1)
	{
		close(server_sockfd);
		exit(1);
	}
	if (listen(server_sockfd, 1024) == -1) //err: may be 1023
	{
		close(server_sockfd);
		exit(1);
	}
	return server_sockfd;
}

// Accept incoming connections
int client_connect(int socket_id) {
	int new_server_sockfd;
	struct sockaddr_in client_addrinfo;
	socklen_t sin_size = sizeof(client_addrinfo);
	new_server_sockfd = accept(socket_id, (struct sockaddr *)&client_addrinfo, &sin_size);
	if (new_server_sockfd == -1)
	{
		close(socket_id);
		exit(1);
	}
	return new_server_sockfd;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
	{
		printf("Use 2 cli arguments\n");
		return -1;
	}
	
	// extract the address and port from the command line arguments
	char serverIP[INET6_ADDRSTRLEN];
	unsigned int server_port;
	strcpy(serverIP, argv[1]);
	server_port = atoi(argv[2]);
	memset(arr,0,sizeof(arr));
    memset(history,0,sizeof(history));
    grp_cnt=0;
    // fp=fopen("./his.txt","w+");
    sem_init(&grps_sem, 0, 1);
    sem_init(&his_sem, 0, 1);

    for (int i = 0; i < 1024; i++) {
        arr[i].sockid = 0;
        arr[i].status = 0;
        arr[i].tot_grps = 0;
        memset(arr[i].username, 0, sizeof(arr[i].username));
        memset(arr[i].my_grps, 0, sizeof(arr[i].my_grps));
    }
    memset(grps,0,sizeof(grps));
	int send_count,recv_count;
	socket_id = create_connection(serverIP, server_port);
    pthread_t tid1;
	pthread_create(&tid1,NULL,make_conn,NULL);
    pthread_join(tid1, NULL); // Blocked here
    close(socket_id);
    return 0;    
}

void* make_conn(void* param){
    int i=0;
    while(1){
        int recv_count;
        arr[i].sockid = client_connect(socket_id);
		arr[i].status=1;
        if ((recv_count = recv(arr[i].sockid, arr[i].username, 1024, 0)) == -1){
			close(socket_id);
			exit(1);
		}
        printf("%s\n",arr[i].username);
        fflush(stdout);
        pthread_t tid2;
	    pthread_create(&tid2,NULL,cli_rec,i);
        i++;
    }
}

void* cli_rec(void* param){
    int idx=param;
    while(arr[idx].status==1){
        int recv_count, send_count;
        char msg[1024];
        memset(msg,0,1024);
        if ((recv_count = recv(arr[idx].sockid, msg, 1024, 0)) == -1){
			close(socket_id);
			exit(1);
		}
        // printf("%s",msg);
        // fflush(stdout);
        // fprintf(fp,"%s-%s",arr[idx].username,msg);
        // fflush(fp);
        sem_wait(&his_sem);
        strcat(history,arr[idx].username);
        strcat(history,"-");
        strcat(history,msg);
        sem_post(&his_sem);
        if(strcmp(msg,"LIST\n")==0){
            char lst[1024]; //err: size may be bigger
            memset(lst,0,1024);
            for(int i=0;i<1024;i++){
                if(arr[i].status==1){
                    strcat(lst,arr[i].username); //err
                    strcat(lst,":");
                }
            }
            lst[strlen(lst)-1]='\n';
            if ((send_count = send(arr[idx].sockid, lst, strlen(lst), 0)) == -1){
                close(socket_id);
                exit(1);
            }
        }
        else if(strncmp(msg,"MSGC:",5)==0){
            char* token=strtok(msg,":");
            char* rec_usr=strtok(NULL,":");
            char* messge=strtok(NULL,"\n");
            int checker=0;
            int to_send;
            for(int i=0;i<1024;i++){
                if((strcmp(rec_usr,arr[i].username)==0)&&arr[i].status==1){
                    checker=1;
                    to_send=arr[i].sockid;
                }
            }
            // printf("DANG\n");
            // fflush(stdout);
            char reply[1024];
            memset(reply,0,1024);
            if(checker==1){
                strcat(reply,arr[idx].username); //err
                strcat(reply,":");
                strcat(reply,messge);
                strcat(reply,"\n");
                if ((send_count = send(to_send, reply, strlen(reply), 0)) == -1){
                    close(socket_id);
                    exit(1);
                }
            }
            else{
                strcat(reply,"USER NOT FOUND\n");
                if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
                    close(socket_id);
                    exit(1);
                }
            }
            
        }
        else if(strncmp(msg,"GRPS:",5)==0){
            char msg_cpy[1024];
            memset(msg_cpy,0,1024);
            strcpy(msg_cpy,msg);
            char* token=strtok(msg_cpy,":");
            char* grp_mem=strtok(NULL,",");
            int check=1;
            char grpname[1024];
            memset(grpname,0,1024);
            while (grp_mem!=NULL&&check==1){
                char* grp_mem_cpy=strtok(NULL,",");
                if(grp_mem_cpy==NULL){
                    char* grp_mem_tmp=strtok(grp_mem,":");
                    grp_mem=grp_mem_tmp;
                    grp_mem_tmp=strtok(NULL,"\n");
                    strcpy(grpname,grp_mem_tmp);
                }
                // if(grp_mem_cpy!=NULL){
                int i=0;
                printf("%s\n",grp_mem);
                for (; i < 1024; i++){
                    if(strcmp(arr[i].username,grp_mem)==0){
                        if(arr[i].status!=1){
                            check=0;
                        }
                        break;
                    }
                }
                if(i==1024){
                    check=0;
                }
                // }
                grp_mem=grp_mem_cpy;
            }
            if(check==0){
                char reply[1024];
                memset(reply,0,1024);
                strcpy(reply,"INVALID USERS LIST\n");
                if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
                    close(socket_id);
                    exit(1);
                }
            }
            else{
                sem_wait(&grps_sem);
                for(int j=0;j<grp_cnt;j++){
                    if(strcmp(grps[j],grpname)==0){
                        strcpy(grps[j],"N O");
                    }
                }
                strcpy(grps[grp_cnt],grpname);
                char* token=strtok(msg,":");
                char* grp_mem=strtok(NULL,",");
                char grpname[1024];
                memset(grpname,0,1024);
                while (grp_mem!=NULL){
                    char* grp_mem_cpy=strtok(NULL,",");
                    if(grp_mem_cpy==NULL){
                        char* grp_mem_tmp=strtok(grp_mem,":");
                        grp_mem=grp_mem_tmp;
                        grp_mem_tmp=strtok(NULL,"\n");
                        strcpy(grpname,grp_mem_tmp);
                    }
                    int i=0;
                    printf("%s\n",grp_mem);
                    for (; i < 1024; i++){
                        if(strcmp(arr[i].username,grp_mem)==0){
                            if(arr[i].status==1){
                                arr[i].my_grps[arr[i].tot_grps]=grp_cnt; 
                                arr[i].tot_grps++;
                            }
                            break;
                        }
                    }
                    grp_mem=grp_mem_cpy;
                }
                grp_cnt++;
                sem_post(&grps_sem);
                char reply[1024];
                memset(reply,0,1024);
                strcat(reply,"GROUP ");
                strcat(reply,grpname);
                strcat(reply," CREATED\n");
                if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
                    close(socket_id);
                    exit(1);
                }
            }
        }
        else if(strncmp(msg,"MCST:",5)==0){
            int check=0;
            char* token=strtok(msg,":");
            char* dst_grp=strtok(NULL,":");
            int grp_idx;
            sem_wait(&grps_sem);
            for(int i=0;i<grp_cnt;i++){
                if(strcmp(grps[i],dst_grp)==0){
                    check=1;
                    grp_idx=i;
                    break;
                }
            }
            sem_post(&grps_sem);
            if(check==0){
                char reply[1024];
                memset(reply,0,1024);
                strcat(reply,"GROUP ");
                strcat(reply,dst_grp);
                strcat(reply," NOT FOUND\n");
                if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
                    close(socket_id);
                    exit(1);
                }
            }
            else{
                char reply[1024];
                memset(reply,0,1024);
                char* mssg=strtok(NULL,":");
                strcat(reply,arr[idx].username);
                strcat(reply,":");
                strcat(reply,mssg);
                for(int i=0;i<1024;i++){
                    if(arr[i].status==1){
                        for(int j=0;j<arr[i].tot_grps;j++){
                            if(arr[i].my_grps[j]==grp_idx){
                                if ((send_count = send(arr[i].sockid, reply, strlen(reply), 0)) == -1){
                                    close(socket_id);
                                    exit(1);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        else if(strncmp(msg,"BCST:",5)==0){
            char* token=strtok(msg,":");
            char* messge=strtok(NULL,"\n");
            char reply[1024];
            memset(reply,0,1024);
            strcat(reply,arr[idx].username);
            strcat(reply,":");
            strcat(reply,messge);
            strcat(reply,"\n");
            for(int i=0;i<1024;i++){
                if(idx!=i&&arr[i].status==1){
                    if ((send_count = send(arr[i].sockid, reply, strlen(reply), 0)) == -1){
                        close(socket_id);
                        exit(1);
                    }
                }
            }
        }
        else if(strcmp(msg,"HIST\n")==0){
            // char reply[10000];
            // char buff[1024];
            // memset(reply,0,10000);
            // memset(buff,0,1024);
            // fseek(fp,0,SEEK_SET); //err: Should be multithreaded?
            // while(fgets(buff,sizeof(buff),fp)!=NULL){
            //     strcat(reply,buff);
            // }
            // if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
            //     close(socket_id);
            //     exit(1);
            // }
            sem_wait(&his_sem);
            if ((send_count = send(arr[idx].sockid, history, strlen(history), 0)) == -1){
                sem_post(&his_sem);
                close(socket_id);
                exit(1);
            }
            sem_post(&his_sem);
        }
        else if(strcmp(msg,"EXIT\n")==0){
            arr[idx].status=0;
        }
        else{
            char reply[1024];
            memset(reply,0,1024);
            strcpy(reply,"INVALID COMMAND\n");
            if ((send_count = send(arr[idx].sockid, reply, strlen(reply), 0)) == -1){
                close(socket_id);
                exit(1);
            }
        }
    }
    pthread_exit(0);
}

//submission format, semaphore for arr, file open close, no file?, semaphore for file