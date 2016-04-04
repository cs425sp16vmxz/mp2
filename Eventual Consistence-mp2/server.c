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
// #include <conio.h>//stdin nonblocking lib
#define MESG_SIZE (256)

int min_delay;
int max_delay;
char **server;
int w;
int r;
int send_counter;
char message_seq[2];
int counter;
int my_id;
int server_size;
int round_counter;
char send_port[5];
int *flag;
int send_index;
char *outfile;
pthread_mutex_t mutex;
pthread_mutex_t send_m;
pthread_cond_t cv;
char read_value[24];
#define MESG_SIZE (256)

/* linked list data struct to save message*/
struct dict
{
    char *key; /* name of key */
    char *value;     //value 
    struct dict *next; /* link field */
};

struct dict * data;

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
/*determine current system time*/
long get_systime()//return 13 char long number
{
  struct timeb tmb;
  ftime(&tmb);
  //printf("tmb.time     = %ld (seconds)\n", tmb.time);
  //printf("tmb.millitm  = %d (mlliseconds)\n", tmb.millitm);
  long sys_time = tmb.time * 1000;
  sys_time += tmb.millitm;
  //printf("time is %ld\n", sys_time);
  return sys_time;
}
//mode 0 get, 1 put
//client id_seq_op_var_systemtime_value 25bit long
/*serializer the message and cap with id, messeger sequence, variable, value, and system time*/
char * serizer(char* message, int * client_id, int mode)
{
	if (mode == 0)//get
	{
		char * ret_message = calloc(8,1);
		strcpy(ret_message, "          ");
		char temp[2];
		//printf("id is %d\n", client_id);
  		snprintf(temp, 2, "%d", (*client_id));
  		//printf("client_id is %d\n", *client_id);
		ret_message[0] = temp[0];
		ret_message[2] = message_seq[0];
		ret_message[4] = message[0];
		ret_message[6] = message[1];
		//printf("message is %s and size of %d\n", ret_message, (int) strlen(ret_message));
		//printf("serizer get message is %s\n", ret_message);
		return ret_message;
	}
	else //put: px2
	{
		long cur_time = get_systime();
		char temp[15];
  		snprintf(temp, 14, "%ld", cur_time);
		char * ret_message = calloc(25,1);
		strcpy(ret_message, "                    ");
		char temp1[2];
  		snprintf(temp1, 2, "%d", (*client_id));
		ret_message[0] = temp1[0];
		ret_message[2] = message_seq[0];
		ret_message[4] = message[0]; //p
		ret_message[6] = message[1]; //x
		ret_message[8] = message[2]; //2
		strncpy(ret_message+10, temp, 14);
		//printf("serizer put message is %s\n", ret_message);
		return ret_message;
	}
}

/*print key and value data in the server*/
void print_local()
{
    struct dict * temp = data;
	while(temp !=NULL)
	{
		char value[2];
		strncpy(value, temp->value+8, 1);
		value[1]=0;
		printf("Key is %s, value is %s\n", temp->key, value);
		temp = temp->next; 
	}
}
/*add data to data linked list sturcture */
void data_add(char * message)
{
	struct dict * temp = calloc(1, sizeof(struct dict));
	temp->key = calloc(1,2);
	strncpy(temp->key, message+6, 1);
	temp->value = calloc(strlen(message)+1, 1);
	strcpy(temp->value, message);
	temp->next = data;
	data = temp;
}
/*find value with given key, NULL: not found*/
char * data_lookup(char * key)
{
	struct dict * temp = data;
	while (temp != NULL)
	{
		if (strncmp(temp->key, key, 1) == 0)
		{
			return temp->value;
		}
		temp = temp->next;
	}
	return NULL;
}
/*compare if the current value is up to date
	return 0 if most up-to-date
	return 1 if need update the server value
*/
int compare(char * local_message, char* server_message)
{
	double local_time;
	sscanf(local_message+10, "%lf", &local_time);
	//snprintf(local_time, 14, "%ld", local_time+10);
	double mes_time;
	sscanf(server_message+10, "%lf", &mes_time);
	//snprintf(mes_time, 14, "%ld", buffer+10);
	if (local_time > mes_time)
		return 0;
	return 1;
}

/*unisend to one server with given port*/
void* unisend(void * arg)
{
	pthread_detach(pthread_self());
	char * message = (char *) arg;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    pthread_mutex_lock(&send_m);
    if (send_index == my_id-1)
    {
    	send_index++;
    	pthread_mutex_unlock(&send_m);
    	return NULL;
    }
    //printf("server file index contains %s\n", server[send_index]);
    char * send_port = server[send_index]+12;
    send_index++;
    pthread_mutex_unlock(&send_m);
    int get_check = getaddrinfo("127.0.0.1", send_port, &hints, &result);
    if (get_check !=0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_check));
    }
    if ( -1 == connect(sock_fd, result->ai_addr, result->ai_addrlen))
   	{
   		//puts("connection false");
   		return NULL;
   	}
   	usleep(((rand() % (max_delay + 1 - min_delay)) + min_delay) * 1000);
   	write(sock_fd, message, strlen(message));
   	char buffer[MESG_SIZE];
   	int len = read(sock_fd, buffer, MESG_SIZE-1);
   	if (len !=-1)
   	{
   		buffer[len] = 0;
   		fflush(stdout);
   		pthread_mutex_lock(&mutex);
   		char seq[2];
		strncpy(seq, buffer+2, 1);
		int index = atoi(seq);
		flag[index]++;
		if (compare(read_value, buffer) == 1)
			strcpy(read_value, buffer);
		pthread_cond_broadcast(&cv);
		pthread_mutex_unlock(&mutex);
   	}

   	return NULL;
}
/*multicast send to all the servers*/
void msend(char * message)
{
	pthread_mutex_lock(&send_m);
	send_index=0;
	pthread_mutex_unlock(&send_m);
	for (int i =0; i < server_size; i++)
	{
		pthread_t pthread_send_id;
		pthread_create(&pthread_send_id, NULL, unisend, (void*) message);
	}

}
//only need id and var from message
/*
write the client request to output file formate .txt
output prototype
		012345678901234567890123456789012
mode 0: 555,2,get,y,1456940000000,req, 	30bit 
mode 1: 555,2,get,y,1456940000400,resp,4 	32bit
mode 2: 555,0,put,x,1456940006000,req,7
mode 3: 555,0,put,x,1456940006250,resp,7
*/
void write_file(char * message, int mode)
{
	FILE *file = fopen(outfile, "a");
    if (file == NULL)
    	puts("open failed");
    char *mes_temp = calloc(34,1);
    strcpy(mes_temp, "555,0,ooo,o,1456940000000,");
    long cur_time = get_systime();
	char temp[15];
  	snprintf(temp, 14, "%ld", cur_time);
  	temp[14]=0;
  	strncpy(mes_temp+4, message,1); //ID
  	strncpy(mes_temp+10, message+6, 1); //Var
  	strncpy(mes_temp+12, temp,13); //sys_time
  	if (mode ==0)
  	{
  		strncpy(mes_temp+6, "get",3);
  		strncpy(mes_temp+26, "req,", 4);
  	}
  	if (mode ==1)	
  	{
  		strncpy(mes_temp+6, "get",3);
  		strncpy(mes_temp+26, "resp,", 5);
  		strncpy(mes_temp+31, message+8, 1);//value
  	}
  	if (mode ==2)
  	{
  		strncpy(mes_temp+6, "put",3);
  		strncpy(mes_temp+26, "req,", 4);
  		strncat(mes_temp, message+8, 1);
  	}
  	if (mode ==3)
  	{
  		strncpy(mes_temp+6, "put",3);
  		strncpy(mes_temp+26, "resp,", 5);
  		strncat(mes_temp, message+8, 1);
  	}
  	fprintf(file, "%s",mes_temp);
  	fputs("\n", file);
  	fclose(file);

}
				//get: 5 0 g x
            	//put: 5 2 p x 2 1459655135867
            	//     01234567890123456789012 24bit long
/*server to server communication, on get and put */
void inter_server_chanel(char * buffer, int * client_fd )
{
	char * message;
    if (strncmp(buffer, "g", 1) == 0)
    {
    	//puts("call inter get");
		pthread_mutex_lock(&mutex);
   		// long init_time = get_systime()
        message = serizer(buffer, client_fd, 0);
        message_seq[0]++;
        char *  ret = data_lookup(message+6);
        write_file(ret, 0);
        //ret[2]++;
        strncpy(ret+2, message+2,1);
        char seq[2];
		strncpy(seq, ret+2,1);
		int index = atoi(seq);
		if (ret !=NULL)
		{
			strcpy(read_value, ret);
        	flag[index]++;
		}
		if (ret == NULL)
			puts("key not found inter server");
		msend(message);
      	while(flag[index] != r)
      		pthread_cond_wait(&cv, &mutex);
      	pthread_mutex_unlock(&mutex);
      	write_file(read_value, 1);
      	write((*client_fd), read_value+8, 1);
    }
    else if (strncmp(buffer, "p", 1) ==0)
    {
        pthread_mutex_lock(&mutex);
   		// long init_time = get_systime()
        message = serizer(buffer, client_fd, 1);
        message_seq[0]++;
        char seq[2];
		strncpy(seq, message+2,1);
		int index = atoi(seq);
		char * temp = data_lookup(message+6);
		if (temp == NULL)
		{
			data_add(message);
			write_file(message, 2);
		}
		if (temp != NULL)
		{
			strcpy(temp, message);
			write_file(message, 2);
		}
		flag[index]++;
		//if (flag[index] != w)
			msend(message);
      	while(flag[index] != w)
      		pthread_cond_wait(&cv, &mutex);
      	pthread_mutex_unlock(&mutex);
      	write_file(message, 3);
      	write((*client_fd), "A", 1);
    }
    else 
    	puts("invalid user input");
}

/* handle the request from client request*/
void * processClient(void * arg)
{
	int client_fd = (intptr_t) arg;
	pthread_detach(pthread_self()); // no join() required
	while (1)
	{
        //int len = read(client_fd, buffer, MESG_SIZE-1);
        char * buffer = calloc(1, MESG_SIZE);
   		int len = read(client_fd, buffer, MESG_SIZE-1);
        if (len != 0)
        {
        	//printf("server received is %s\n", buffer);
        	if (!strncmp(buffer, "g", 1) || !strncmp(buffer, "p", 1) )
        	{	

        		inter_server_chanel(buffer, &client_fd);
        	}
        	else if (!strncmp(buffer, "d", 1))
        	{
        		print_local();
        		write(client_fd, "A", 1);
        	}
            else
            {
            	//get: 5 0 g x
            	//put: 5 2 p x 2 1459655135867
            	//     01234567890123456789012 24bit long
            	if (strncmp(buffer+4, "g", 1) == 0)
            	{
            		//printf("receive from server message is %s\n", buffer);
            		char var[2];
            		strncpy(var, buffer+6, 1);
            		char * ret = data_lookup(var);
            		strncpy(ret+2, buffer+2,1);
            		if (ret != NULL)
            		{
            			//printf("my local read %s\n", ret);
            			write(client_fd, ret, strlen(ret));
            		}
            		else
            			puts("key not found");           		
            	}
            	if (strncmp(buffer+4, "p", 1) == 0)
            	{
            		char var[2];
            		strncpy(var, buffer+6, 1);
            		char * ret = data_lookup(var);
            		if (ret == NULL)
            		{
            			data_add(buffer);
            			write(client_fd, buffer, strlen(buffer));
            		}
            		else 
            		{
            			if (compare(ret, buffer) ==0)
            				write(client_fd, ret, strlen(ret));
            			else
            			{
            				strcpy(ret, buffer);
            				write(client_fd, ret, strlen(ret));
            			}
            		}
            	}
            	break;
            }
       	}
    }
    return NULL;
}


/* 
main function to initialize the function and start a new thread for each client.
   ./server config ID W  R NumOfServer; 
eg ./server config 1  2  3     5
*/
int main(int argc, char** argv)
{
    if (argc != 6)
    {
       perror("invalid argv\n");
       exit(1);
    }
    server= calloc(10,sizeof(char*));
	configParser(argv[1]);
	// for (int i =0; i<9; i++)
	// {
	// 	printf("server list is %s\n", server[i]);
	// }
	my_id = atoi(argv[2]);
	w = atoi(argv[3]);
	r = atoi(argv[4]);
	server_size = atoi(argv[5]);
	outfile = calloc(16,1);
	strcpy(outfile, "output_log0.txt");
	strncpy(outfile+10, argv[2], 1);
	//printf("output_log file name %s\n", outfile);
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int get_check = getaddrinfo(NULL, server[my_id-1]+12, &hints, &result);
    if (get_check !=0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(get_check));
    }
    if(bind(sock_fd, result->ai_addr, result->ai_addrlen)!=0)
    {
        perror("bind() error\n");
        exit(1);
    }
    if(listen(sock_fd, 10) !=0 )
    {
        perror("listen() error\n");
        exit(1);
    }
    message_seq[0] = '0';
    message_seq[1] = 0;
    flag = calloc(100,sizeof(int));
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&send_m, NULL);
    send_index =0;
    pthread_cond_init(&cv, NULL);
    data = NULL;
    while(1)
    {
    	int fd= accept(sock_fd, NULL, NULL);
    	if (fd != -1)
    	{
    		pthread_t pthread_ID;
    		pthread_create(&pthread_ID, NULL, processClient, (void *) fd);
    	}
    }
}




















