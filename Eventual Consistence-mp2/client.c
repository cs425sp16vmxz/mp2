//auther Xiaobin Zheng, xzheng19, 668374748
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <arpa/inet.h>
//#include <fcntl.h>
#include <sys/timeb.h>

int min_delay;
int max_delay;
int server_index;
pthread_mutex_t mutex;
int flag;
char **server;
char send_port[5];
#define MESG_SIZE (256)

/*Config file parser, to extact parameters. eg, delay range, server ports*/
void configParser(const char *filename)
{
	FILE * file = fopen(filename, "r");
	if (file == NULL)
        exit(EXIT_FAILURE);

	char * line = NULL;
	size_t buf_len = 0;
	int index = -1;
	while (getline(&line, &buf_len, file) != -1)
	{
		line[strlen(line)-1] =0;
		//printf("line is %s\n", line);
		if (index == -1)
		{
			 char *ret_s = strstr(line, "(");
			 char *ret_e = strstr(line, ")");
			 (*ret_e) = 0;
			 min_delay = atoi(ret_s+1);
			 ret_s = strstr(ret_e+1, "(");
			 ret_e = strstr(ret_e+1, ")");
			 (*ret_e) = 0;
			 max_delay = atoi(ret_s+1);
			 index++;
		}
		else 
		{
			server[index] = calloc(1, buf_len+1);
			strcpy(server[index], line);
			index++;
		}
	}
}

/*server channel function to handle client and server communication*/
void * server_channel(void * arg)
{
	int fd = (intptr_t) arg;
	while (1)
	{
		char buffer[MESG_SIZE];
		int len = read(fd, buffer, MESG_SIZE-1);
        if (len != 0)
        {

            buffer[len] ='\0';
            if (strncmp(buffer, "A", 1) == 0)
            {
            	puts("A");
            }
            else
            {
            	//printf("value is %d\n", atoi(buffer));
            	printf("%d\n", atoi(buffer));
            	fflush(stdout);
            }            
       	}

       	/*flag for the server is status. 
       	Down: flag = 1 active flag =0
       	*/
       	if (len == 0)
       	{
       		pthread_mutex_lock(&mutex);
       		flag=1;
       		pthread_mutex_unlock(&mutex);
       		return NULL;
       	}
	}	
}
/*
 the connection port increase by one to reconnect to a new server when previous server is down.
*/
void reconnect_server(char * line){
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int get_check = getaddrinfo("127.0.0.1", server[server_index+1]+12, &hints, &result);
    if (get_check !=0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_check));
    }
    if ( -1 == connect(sock_fd, result->ai_addr, result->ai_addrlen))
   	{
   		puts("connection false");
   	}
   	pthread_t threadID;
    pthread_create(&threadID, NULL, server_channel, (void *) sock_fd);

    		if (strncmp("get", line, 3) == 0)
			{
				char * message = calloc(1, 3);
				strncpy(message, "g",1);
				strncat(message, line+4,1);	
				write(sock_fd, message, strlen(message));
			}
			if (strncmp("put", line, 3) == 0)
			{
				char * message = calloc(1, 4);
				strncpy(message, "p",1);
				strncat(message, line+4,1);
				strncat(message, line+6,1);
				write(sock_fd, message, strlen(message));
			}
	while(1)
	{
		char *line = NULL;
  		size_t buf_len = 0;
  		if (getline(&line, &buf_len, stdin) != -1)
  		{
			if (strncmp("get", line, 3) == 0)
			{
				char * message = calloc(1, 3);
				strncpy(message, "g",1);
				strncat(message, line+4,1);
				//client_send(message);	
				write(sock_fd, message, strlen(message));
			}
			else if (strncmp("put", line, 3) == 0)
			{
				char * message = calloc(1, 4);
				strncpy(message, "p",1);
				strncat(message, line+4,1);
				strncat(message, line+6,1);
				//client_send(message);
				write(sock_fd, message, strlen(message));
			}
			else if (strncmp("delay", line, 5) == 0)
			{
				line[buf_len] = 0;
				int ms = atoi(line+6);
				//printf("delay is %d\n", ms); 
				usleep(ms*1000);
			}

			else if (strncmp("dump", line, 4) == 0)
			{
				char * message = calloc(1, 2);
				strncpy(message, "d", 1);
				write(sock_fd, message, strlen(message));
			}
			else 
				perror ("invalid command input\n");	
  		}
	}
}
/*  
	main function to connect to the server with given port number.
	./client config ID serversize
e.g ./client config 1 3
*/
int main(int argc, char *argv[])
{
	pthread_mutex_init(&mutex, NULL);
	server= calloc(10,sizeof(char*)); //all possive server 
	configParser(argv[1]);
	server_index = atoi(argv[2])-1;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int get_check = getaddrinfo("127.0.0.1", server[server_index]+12, &hints, &result);
    if (get_check !=0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_check));
    }
    if ( -1 == connect(sock_fd, result->ai_addr, result->ai_addrlen))
   	{
   		puts("connection false");
   	}
   	pthread_t threadID;
    pthread_create(&threadID, NULL, server_channel, (void *) sock_fd);

    char *line;
	while(1)
	{
		line = NULL;
  		size_t buf_len = 0;
  		if (getline(&line, &buf_len, stdin) != -1)
  		{
  			/*check to see if server is active*/
  			pthread_mutex_lock(&mutex);
			if (flag ==1)
			{
				pthread_mutex_unlock(&mutex);
				break;
			}
			pthread_mutex_unlock(&mutex);

			if (strncmp("get", line, 3) == 0)
			{
				char * message = calloc(1, 3);
				strncpy(message, "g",1);
				strncat(message, line+4,1);
				//client_send(message);	
				write(sock_fd, message, strlen(message));
			}
			else if (strncmp("put", line, 3) == 0)
			{
				char * message = calloc(1, 4);
				strncpy(message, "p",1);
				strncat(message, line+4,1);
				strncat(message, line+6,1);
				//client_send(message);
				write(sock_fd, message, strlen(message));
			}
			else if (strncmp("delay", line, 5) == 0)
			{
				line[buf_len] = 0;
				int ms = atoi(line+6);
				//printf("delay is %d\n", ms); 
				usleep(ms*1000);
			}

			else if (strncmp("dump", line, 4) == 0)
			{
				char * message = calloc(1, 2);
				strncpy(message, "d", 1);
				write(sock_fd, message, strlen(message));
			}
			else 
				perror ("invalid command input\n");	
  		}
	}
	close(sock_fd);
	//reconnect to server
	reconnect_server(line);//reconnect to a new server

	return 1;

}































