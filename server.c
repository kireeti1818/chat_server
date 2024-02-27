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

#define PORT 18185

// #include <stdio.h>
// #include <openssl/sha.h>
// #include <openssl/bio.h>
// #include <openssl/buffer.h>

// // Function to base64 encode a given data
// char* base64_encode(const unsigned char* input, int length) {
//     BIO *bio, *b64;
//     BUF_MEM *bufferPtr;
//     b64 = BIO_new(BIO_f_base64());
//     BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
//     bio = BIO_new(BIO_s_mem());
//     bio = BIO_push(b64, bio);
//     BIO_write(bio, input, length);
//     BIO_flush(bio);
//     BIO_get_mem_ptr(bio, &bufferPtr);
//     char* encodedData = (char*)malloc(bufferPtr->length + 1);
//     memcpy(encodedData, bufferPtr->data, bufferPtr->length);
//     encodedData[bufferPtr->length] = '\0';
//     BIO_free_all(bio);
//     return encodedData;
// }

// int main() {
//     unsigned char hash[SHA_DIGEST_LENGTH];
//     char* encoded_hash;

//     // Assuming `data` is the data you want to hash
//     const char* data = "your_hash_data";

//     // Compute SHA-1 hash
//     SHA1(data, strlen(data), hash);

//     // Encode the hash using base64
//     encoded_hash = base64_encode(hash, SHA_DIGEST_LENGTH);

//     printf("%s\n", encoded_hash);  // Print the encoded hash

//     free(encoded_hash); // Free dynamically allocated memory
//     return 0;
// }



int main(int argc, char const *argv[])
{

	char *address="127.0.0.1";

	//creating socket
	int sockfd=socket(AF_INET,SOCK_STREAM,0);
	if (sockfd<0)
	{
		printf("error in socket discriptor\n");
		return 0;
	}
	printf("socket created\n");

	//enable reuse of local addresses 
	int yes=1;
	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)
	{
		printf("setsockopt");
		exit(0);
	}


	struct sockaddr_in server,client;
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
	printf("port binded to localhost \n\n");

	//make the specified socket a listening socket, allowing it to accept incoming connections.
	//'1' indicates the maximum length of the queue of pending connections
	int lis=listen(sockfd,1);
	if(lis<0)
	{
   		perror("listen");
        exit(0);
    }


    //allowing it to accept incoming connections.
    socklen_t len=sizeof(client);
    int acc;
    if ((acc = accept(sockfd, (struct sockaddr *)&client, &len)) < 0)
    {
        printf("error while accepting");
        return 0;
    }
    printf("accepting the connection........\n");


    char buff[10000];
    memset(&buff,'\0',1000);

    //receving message from the server
    int rec;
    rec=recv(acc,buff,1000,0);
    buff[strlen(buff)-1]='\0';
    // printf("%s",buff);

    char * key = strstr(buff,"Sec-WebSocket-Key");
    printf("%s\n",key);
    
    char * start = strstr(key,":");
    char * end = strstr(start,"\n");
    start=start+2;
    // printf("%s\n",start);
    int size = end-start;
    // printf("%d",size);
    char websock_key[size];
    int ind=0;
    while(start!=end)
    {
        websock_key[ind++]=*start++;

    }
    websock_key[ind]='\0';
    printf("%s",websock_key);
}