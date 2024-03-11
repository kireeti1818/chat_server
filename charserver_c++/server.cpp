#include <bits/stdc++.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <errno.h>
#include <signal.h>
#include <thread>
#include <mutex>
#include <unordered_map> 
#include <cstring> 
#include <cstdlib> 

using namespace std;

pthread_mutex_t client_fds_mutex = PTHREAD_MUTEX_INITIALIZER;
unordered_map <int , string> clients;
string webSocketGUID= "258EAFA5-E914-47DA-95CA-C5AB0DC85B11\0"; 
string address="127.0.0.1";
uint16_t  PORT = 18185;
int sockfd;


class ping_pong_handler
{
    protected:
        void handle_ping(uint8_t * buffer,long long length,int fd)
        {
            uint8_t pong_frame[128];
            pong_frame[0]= 0xA;
            pong_frame[1]= *(buffer+1);
            memcpy(pong_frame + 2, buffer + 2, length - 2);
            send(fd, pong_frame, length, 0);
        }
};


class encoding_decoding : private ping_pong_handler{
    public:
        void Base64Encode(char *client_key, char *accept_key) 
        { 

            char combined_key[1024];
            strcpy(combined_key, client_key);
            strcat(combined_key, webSocketGUID.c_str());
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
        void mask_extract(uint8_t* mask_Key ,int index,uint8_t *buffer)
        {
            for(int i =index;i<index+4;i++)
            {
                *mask_Key = static_cast<char>(buffer[i]); // Cast to char
                mask_Key++;
            }

        }
        int encode_websocket_frame (uint8_t fin,uint8_t opcode,uint64_t payload_length, uint8_t *payload,uint8_t *frame) 
        {
            int header_size = 2;
            frame[0] = (fin << 7) | (opcode & 0x0F); 
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
            memcpy (frame + header_size, payload, payload_length);
            return header_size + payload_length; 
        }
        uint8_t * decode_websocket_frame(uint8_t  *buffer, int len,int fd)
        {
            int firstByte = *(buffer+1);
            int n = 0x80;
            int mask = firstByte & n;
            uint8_t *message;
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
};




class send_frame 
{
    encoding_decoding en_de;
    public:
        int send_websocket_frame (int client_socket, uint8_t fin, uint8_t opcode, char *payload) 
        {
            uint8_t encoded_data [1024];
            int encoded_size = en_de.encode_websocket_frame (fin, opcode, strlen (payload), (uint8_t *)payload, encoded_data);

            ssize_t bytes_sent = send (client_socket, encoded_data, encoded_size, 0);
            if (bytes_sent == -1) 
            {
                perror ("Send failed");
                return -1;
            }
            return 0;
        }
};

class username 
{
    encoding_decoding en_de;
    send_frame send;
    private:
        bool username_checker(int client_socket,string userNameToFind)
        {
            bool found = false;

            for (const auto& pair : clients) {
                if (pair.second == userNameToFind) {
                    found = true;
                    break;
                }
            }
            if (found) {
                send.send_websocket_frame (client_socket ,1, 1,const_cast<char*>("__false__"));
                printf("USER NAME status __false__\n");
                return false;
            } else {
                return true;
            }
        }
    protected:
        string userName;
        string username_setter(int client_socket)
        {

            do {
                uint8_t buffer[27];
                int received = recv(client_socket, buffer, 1000, 0);
                if(received==0) {
                    return "";
                }
                buffer[received] = '\0'; 
                userName = reinterpret_cast<char*>(en_de.decode_websocket_frame(buffer, received, client_socket));
            } while (!username_checker(client_socket,userName));
            clients[client_socket] = userName;
            printf("USER NAME status __success__\n");
            printf("username set to %s \n\n", userName.c_str());
            send.send_websocket_frame (client_socket ,1, 1,const_cast<char*>("__success__"));

            return userName;
        }


        void remove_sock( int remove_sock) 
        {
            if (clients.find(remove_sock) != clients.end()) {
                // Key exists, erase it
                clients.erase(remove_sock);  
            } 
            close(remove_sock);
        }


};

class Message : private username
{
    encoding_decoding en_de;
    send_frame send;
    private:
        int private_username_checker(int client_socket,string userNameToFind)
        {
            bool flag =false;
            int socketfd;
            for (const auto& pair : clients) 
            {
                if(userNameToFind==pair.second)
                {
                    socketfd=pair.first;
                    flag=true;
                    break;
                }
            }
            if (flag==true) return socketfd;
            else {
                send.send_websocket_frame (client_socket ,1, 1, const_cast<char*>("__usernotfound___"));
                printf("usernotfound\n");
                return -1;
            }
        }
        //function over loading
        uint8_t* concat_with_delimiter(const std::string& str1, const char* str2, const char* delimiter) 
        {
            size_t len1 = str1.length();
            size_t len2 = strlen(str2);
            size_t delim_len = strlen(delimiter);

            uint8_t* result = (uint8_t*)malloc(len1 + delim_len + len2 + 1); 

            if (result == nullptr) {
                std::cout << "Memory allocation failed." << std::endl;
                return nullptr;
            }

            strcpy((char*)result, str1.c_str());
            strcat((char*)result, delimiter);
            strcat((char*)result, str2);

            return result;
        }
        //function over loading
        uint8_t* concat_with_delimiter(const std::string& str1, const char* str2, const char* str3,const char* delimiter) 
        {
            size_t len1 = str1.length();
            size_t len2 = strlen(str2);
            size_t delim_len = strlen(delimiter);

            uint8_t* result = (uint8_t*)malloc(len1 + delim_len + len2 + 1); 

            if (result == nullptr) {
                std::cout << "Memory allocation failed." << std::endl;
                return nullptr;
            }

            strcpy((char*)result, str1.c_str());
            strcat((char*)result, delimiter);
            strcat((char*)result, str2);
            strcat((char*)result, str3);

            return result;
        }
    

    void broadcast_message(int client_socket,string username,uint8_t *message)
    {
        message=concat_with_delimiter(username, (char*)message, " - ");
        for (auto it = clients.begin(); it != clients.end(); ++it) 
        {
            if(it->first!=client_socket)
            {
                
                send.send_websocket_frame(it->first ,1, 1, (char *)message);
            }
        }
    }
    bool private_message(int client_socket,string username,string reciever_username, uint8_t * message)
    {
        
        message=concat_with_delimiter(username, (char*)message, " (private)"," - ");

        int reciever_socket_fd;
        if((reciever_socket_fd=private_username_checker(client_socket,reciever_username))!=-1)
        {

            send.send_websocket_frame(reciever_socket_fd,1, 1, (char *)message);
            return true;

        }
        return false;
    }
    public:
    void * handling_message(void * client_sock)
    {
        
        int client_socket = *((int *)client_sock);
        uint8_t buffer[1000];
        
        string username;
        
        username=username_setter(client_socket);
        if(userName=="")
        {
            
            send.send_websocket_frame (client_socket ,1, 1, const_cast<char*>("__userNameNotSet___"));
            close(client_socket);
            pthread_exit(NULL);
            
        }
        
        while(1)
        {
            int received = recv(client_socket, buffer, 1000, 0);

            if (received <= 0) 
            {
                cout<<clients[client_socket]<<" disconnected\n";
                remove_sock(client_socket); 
                close(client_socket);
                break;
            } 
            else 
            {
                buffer[received] = '\0'; 
                uint8_t * message=en_de.decode_websocket_frame(buffer, received, client_socket);
                if (message==NULL)
                {
                    printf("Connection closed by the client\n");
                    remove_sock(client_socket);
                    close(client_socket);
                    pthread_exit(NULL);
                    return NULL;
                }
                if(strstr(reinterpret_cast<char*>(message),"__broadcast___")!=NULL)
                {
                    message =  (uint8_t*)strstr((char*)message, "___");

                    message+=3;
                    printf("broadcasting %s\n",message);
                    broadcast_message(client_socket,username,message);

                }
                else
                {

                    string reciever_username;
                    message = message + 2; 
                    int i = 0;
                    while (*(message + i) != '_') {
                        reciever_username += *(message + i);
                        i++;
                    }
                    message = message + i + 3;
                    bool flag = private_message(client_socket,username,reciever_username, message);
                    if (flag==true)
                     printf("sending %s to %s\n", message, reciever_username.c_str());
                }

            }
        }
        close(client_socket);
        pthread_exit(NULL);
        return NULL;
    }
};
//over
class Websocket{
private:
    int sockfd;

    int socket_creation() {
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            cerr << "Error in socket descriptor" << endl;
            return 0;
        }
        cout << "[+] Socket created" << endl;
        return sockfd;
    }
    void setSocketReuseAddress()
    {
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            cerr << "setsockopt error" << endl;
            exit(0);
        }
    }
    void binding()
    {
        struct sockaddr_in server;
        memset(&server, '\0', sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(address.c_str());
        server.sin_port = htons(PORT); 
        int bin = bind(sockfd, (struct sockaddr *)&server, sizeof(server));
        if (bin < 0) {
            perror("sockaddr");
            exit(0);
        }
        cout << "[+] Port binded to localhost" << endl;
    }
    void listening()
    {
        int lis = listen(sockfd, 10);
        if (lis < 0) {
            perror("listen");
            exit(0);
        }
    }
     
    public:
        int socket_creation_binding_listening_function()
        {
            Websocket::socket_creation();
            Websocket::setSocketReuseAddress();
            Websocket::binding();
            Websocket::listening();

            return sockfd;
        }
};


class connection 
{
    encoding_decoding en_de;
    private:
        struct sockaddr_in client;
        socklen_t len=sizeof(client);
        int client_socket;
        
    protected:
        //over
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
            char web_sock_key[size];


            int ind=0;
            while(start!=end)
            {
                web_sock_key[ind++]=*start++;

            }
            web_sock_key[ind]='\0';

            char encoded_hash[1024];
            en_de.Base64Encode(web_sock_key, encoded_hash);
            // printf("%s\n", encoded_hash); 

            char response[1125] ;
            sprintf(response,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",encoded_hash);
            printf("[+]client is connecting...\n\nSetting USERNAME\n\n");
            printf("key : %s\n\n",web_sock_key);
            printf("Setting USERNAME\n\n");

            int se=send(client_socket,response,strlen(response),0);
            if(se<0)
            {
                perror("send\n");
                exit(0);
            }
        }
        int client_accept_function()
        {
            while(1)
            {
                if ((client_socket = accept(sockfd, (struct sockaddr *)&client, &len)) < 0)
                {
                    printf("error while accepting");
                    return 0;
                }
        
                handle_connection(client_socket);
                
                pthread_t thread_id;
                
                if (pthread_create(&thread_id, NULL, [](void* arg) -> void* { return Message().handling_message(arg); }, (void *)&client_socket) < 0) {

                // if (pthread_create(&thread_id, NULL, Message::handling_message, (void *)&client_socket) < 0) {
                    perror("could not create thread");
                    exit(EXIT_FAILURE);
                }

            }
        }
};


class ChatServer: private connection{
    public:
    void Startchat(){
        Websocket create;
        sockfd=create.socket_creation_binding_listening_function();
        client_accept_function();
    }
};

int main()
{
    ChatServer chat;
    chat.Startchat();
}
