#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <stdint.h>
#include <pthread.h>

#define PORT 18185
#define MAX_CLIENTS 1000

int client_fds[MAX_CLIENTS] ;

char *webSocketGUID= "258EAFA5-E914-47DA-95CA-C5AB0DC85B11\0"; 
char *address="127.0.0.1";


pthread_mutex_t client_fds_mutex = PTHREAD_MUTEX_INITIALIZER;

//Encodes a binary safe base 64 string
void Base64Encode(char *client_key, char *accept_key) 
{ 


    char combined_key[1024];
    strcpy(combined_key, client_key);
    strcat(combined_key, webSocketGUID);
    //printf("%s\n",combined_key);

    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)combined_key, strlen(combined_key), sha1_hash);

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    BIO *bio = BIO_new(BIO_s_mem());
    BIO_push(b64, bio);

    BIO_write(b64, sha1_hash, SHA_DIGEST_LENGTH);
    BIO_flush(b64);

    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);

    strcpy(accept_key, bptr->data);

    size_t len = strlen(accept_key);
    if (len > 0 && accept_key[len - 1] == '\n') 
    {
        accept_key[len - 1] = '\0';
    }

    BIO_free_all(b64);
}



void handle_ping(uint8_t * buffer,long long length,int fd)
{
    uint8_t pong_frame[128];
    pong_frame[0]= 0xA;
    pong_frame[1]= *(buffer+1);
    memcpy(pong_frame,(buffer+2),length-2);
    send(fd, pong_frame,strlen(pong_frame),0);
}



void mask_extract(char * mask_Key,int index,uint8_t *buffer)
{
    for(int i =index;i<index+4;i++)
    {
        *mask_Key=buffer[i];
        mask_Key++;
    }

}


uint8_t * decode_websocket_frame(uint8_t  *buffer, int len,int fd)
{
    int firstByte = *(buffer+1);
    int n = 0x80;
    int mask = firstByte & n;
    char *message;
    if (mask ==128)
    {
        long long int payload_Length = firstByte & 0x7f;
        uint8_t mask_Key[5];
        mask_Key[4]='\0';
        int index,payload_start;
        if(payload_Length<=125)
        {
            index=2;
            mask_extract(mask_Key,index,buffer);
            payload_start=4+index;
            
        }
        else if(payload_Length==126)
        {
            payload_Length= (*(buffer+2)<<8) | *(buffer+3); 
            index=4;
            mask_extract(mask_Key,index,buffer);
            payload_start=4+index;

        }
        else if(payload_Length==127)
        {
            payload_Length=0;
            for(int j=0;j<8;j++)
            {
                payload_Length <<=8;
                payload_Length |= *(buffer+j+2); 
            }
            
            index=10;
            mask_extract(mask_Key,index,buffer);
            payload_start=4+index;

        }
        // printf("%lld\n",payload_Length);
        uint8_t opcode = buffer[0] & 15;
        if(opcode == 0x9) // ping
        {
            printf("opcode\n");
            handle_ping(buffer,payload_Length,fd);
            return NULL;
        }
        else if(opcode == 0x8)
        {
            close(fd);
            return NULL;
        }
        message = (uint8_t *)malloc((payload_Length + 1) * sizeof(uint8_t));
        // uint8_t message[payload_Length+1];
        message[payload_Length]='\0';
        for(int i=0;i<payload_Length;i++)
        {
            *(message+i)= *(buffer+i+payload_start) ^ mask_Key[i%4];
        }
        printf("\n");
    }
    
    return message;
}

// Function to encode a complete WebSocket frame
int encode_websocket_frame (uint8_t fin,uint8_t opcode,uint64_t payload_length, uint8_t *payload,uint8_t *frame) 
{
    // Calculate header size based on payload length
    int header_size = 2;
    frame[0] = (fin << 7) | (opcode & 0x0F); //129(1000 0001)
    frame[1] = (0 << 7); 
    if(payload_length <= 125){
        frame[1] |= payload_length;
    }else if(payload_length <= 65536){
        frame[1] |= 126;
        frame[2] = (payload_length >>  8) & 0xFF;
        frame[3] = (payload_length      ) & 0xFF;
        header_size += 2;
    }else{
        frame[1] |= 127;
        frame[2] = (payload_length >> 56) & 0xFF;
        frame[3] = (payload_length >> 48) & 0xFF;
        frame[4] = (payload_length >> 40) & 0xFF;
        frame[5] = (payload_length >> 32) & 0xFF;
        frame[6] = (payload_length >> 24) & 0xFF;
        frame[7] = (payload_length >> 16) & 0xFF;
        frame[8] = (payload_length >>  8) & 0xFF;
        frame[9] = (payload_length      ) & 0xFF;
        header_size += 8;
    }

    // Copy payload after header
    memcpy (frame + header_size, payload, payload_length);
    return header_size + payload_length; // Total frame size
}


void handle_connection(int client_socket)
{
    char buff[10000];
    memset(&buff,'\0',1000);

    //receving message from the server
    int rec;
    rec=recv(client_socket,buff,1000,0);
    buff[strlen(buff)-1]='\0';
    // printf("%s",buff);

    char * key = strstr(buff,"Sec-WebSocket-Key");
    
    char * start = strstr(key,":");
    char * end = strstr(start,"\n");
    start=start+2;
    end-=1;
    int size = end-start;
    char websock_key[size];

    int ind=0;
    while(start!=end)
    {
        websock_key[ind++]=*start++;

    }
    websock_key[ind]='\0';

    char encoded_hash[1024];
    Base64Encode(websock_key, encoded_hash);
    // printf("%s\n", encoded_hash); 

    char response[1125] ;
    sprintf(response,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",encoded_hash);
    printf("[+]connection esatablised\n");
    printf("key : %s\n",websock_key);

    int se=send(client_socket,response,strlen(response),0);
    if(se<0)
    {
        perror("send\n");
        exit(0);
    }
}

int socket_creation()
{
    struct sockaddr_in server;
    //creating socket
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	if (sockfd<0)
	{
		printf("error in socket discriptor\n");
		return 0;
	}
	printf("[+]socket created\n");

    //The SO_REUSEADDR option allows a socket to bind to an address that is already in use by another socket.
    // For example, if a server needs to restart after a crash and bind to the same port as the previous instance,
    // using SO_REUSEADDR can prevent the "Address already in use" error without waiting for the OS to release the port.
	int yes=1;
	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
	{
		printf("setsockopt");
		exit(0);
	}

	memset(&server,'\0',sizeof(server));
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=inet_addr(address);
	server.sin_port=htons(PORT);

	//associates socket with a specific network address and port number.
	int bin =bind(sockfd,(struct sockaddr *)&server,sizeof(server));
	if (bin<0)
	{
		perror("sockaddr");
		exit(0);
	}
	printf("[+]port binded to localhost \n");

	//make the specified socket a listening socket, allowing it to accept incoming connections.
	//'1' indicates the maximum length of the queue of pending connections
	int lis=listen(sockfd,10);
	if(lis<0)
	{
   		perror("listen");
        exit(0);
    }

    return sockfd;
}
int send_websocket_frame (int client_socket, uint8_t fin, uint8_t opcode, char *payload) 
{
    uint8_t encoded_data [1024];
    int encoded_size = encode_websocket_frame (fin, opcode, strlen (payload), (uint8_t *)payload, encoded_data);

    ssize_t bytes_sent = send (client_socket, encoded_data, encoded_size, 0);
    if (bytes_sent == -1) 
    {
        perror ("Send failed");
        return -1;
    }
    return 0;
}
void broadcast_message(int client_socket,uint8_t *message)
{
    pthread_mutex_lock(&client_fds_mutex);
    int i=0;
    while(client_fds[i]!=-1)
    {
        if(client_fds[i]!=client_socket) {
            send_websocket_frame (client_fds[i] ,1, 1, message);
        }
        i++;
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void *handling_message(void * client_sock)
{
    pthread_mutex_lock(&client_fds_mutex);
    uint8_t buffer[1000];
    int client_socket = *((int *)client_sock);
    // printf("%d\n",client_socket);
    for(int ind =0;ind<MAX_CLIENTS;ind++)
    {
        if (client_fds[ind]==-1)
        {
            client_fds[ind]=client_socket;
            break;
        }
    }

    pthread_mutex_unlock(&client_fds_mutex);
    while(1)
    {
        int received = recv(client_socket, buffer, 1000, 0);

        if (received <= 0) 
        {
            printf("Connection closed by the client\n");
            if(client_fds[MAX_CLIENTS-1]==client_socket)
            {
                client_fds[MAX_CLIENTS-1]=-1;
            }
            int flag=0;
            int ind=0;
            for(ind =0;ind<MAX_CLIENTS;ind++)
            {
                if (flag==1 && client_fds[ind+1]!=-1)
                {
                    client_fds[ind]=client_fds[ind+1];
                    continue;
                }
                else if (flag==1 &&  client_fds[ind]==-1) 
                {
                    client_fds[ind]==-1;
                    break;
                }
                else if (ind==MAX_CLIENTS-1) client_fds[MAX_CLIENTS-1]=-1;

                if(client_fds[ind]==client_socket) flag=1;
            }
            close(client_socket);
            break;
        } 
        else 
        {
            buffer[received] = '\0'; 
            uint8_t *message=decode_websocket_frame(buffer, received, client_socket);
            printf("Recieved Message : %s\n",message);
            broadcast_message(client_socket,message);
        }
    }
    close(client_socket);
    pthread_exit(NULL);
    return NULL;
}



char * get_username(int client_socket)
{
    uint8_t buffer[27];
    int received = recv(client_socket, buffer, 1000, 0);
    buffer[received] = '\0'; 
    return decode_websocket_frame(buffer, received, client_socket);

}



int main()
{
    memset(client_fds, 0xFF, sizeof(client_fds));
    struct sockaddr_in client;
	int sockfd = socket_creation(); 
    socklen_t len=sizeof(client);
    int client_socket;
    
    while(1)
    {
        if ((client_socket = accept(sockfd, (struct sockaddr *)&client, &len)) < 0)
        {
            printf("error while accepting");
            return 0;
        }
 
        handle_connection(client_socket);

        char *username=get_username(client_socket);
        printf("%s \n",username);


        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handling_message, (void *)&client_socket) < 0) {
            perror("could not create thread");
            exit(EXIT_FAILURE);
        }

    }
	return 0;

}

