#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

//User defined helper functions
void printMsg(char *print);
void checkDir(DIR *dir, char *folder);
void buildPath(char *folder, char *message);
void error(int errorCode, const char *Errormsg, int sockfd);
void sendPacket(int opCode, int blockNO, int sockfd, char *data, int sizesz);
int argCheck(int port_n, char *folder);

union { unsigned short blocknumber; char bytes[2]; } bit;

//global variables
char file_path[1];
struct sockaddr_in server, client;

int main(int argc, char **argv){
    int port_n = 0, sockfd;
    char *folder;
    DIR *dir = NULL;
    char message[512];
    FILE *fd;
    folder = malloc(sizeof(&argv[2][0]));
    sscanf(&argv[1][0], "%d", &port_n);
    sscanf(&argv[2][0], "%s", folder);

    if(argc != 3){
        //Later change this to present a menu asking for destination n' stuff
        printMsg("Number of arguments incorrect.");
        free(folder);
        return 0;
    } else if(!argCheck(port_n, folder)){
        free(folder);
        return 0;
    }



    //Check if directory is available to server
    checkDir(dir, folder);
    /* Create and bind a UDP socket = SOCK_DGRAM  */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){ 
        perror("cannot create socket");
        return 0;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    /* Network functions need arguments in network byte order instead of host byte order. The macros htonl, htons convert the values, */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port_n);

    if(bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server))){
        perror("bind failed");
        return 0;
    }

    printf("Server is ready and listening on port %d\n", ntohs(server.sin_port));
    fflush(stdout);
    while(1){
        fd_set rfds;
        struct timeval tv;
        int retval;

        /* Check whether there is data on the socket fd. */
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        /* Set socket to wait for five seconds. */
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        if((retval = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                perror("select()");
        } else if (retval > 0) {

            /* Data is available, receive it. */
            assert(FD_ISSET(sockfd, &rfds));
            
            /* Copy to len, since recvfrom may change it. */
            socklen_t len = (socklen_t) sizeof(client);
            /* Receive one byte less than declared, because it will be zero-termianted below. */
            recvfrom(sockfd, message,
                         sizeof(message) - 1, 0,
                         (struct sockaddr *) &client,
                         &len);
            /* check OP code, only allow get */
            int OP = message[1];
            //if( mode == ascii or binary)
            if(OP == 1){
                printf("file %s requested by %s:%d\n", &message[2], inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                /*Build file path argument string and open file*/
                buildPath(folder, message);
                if((fd = fopen(file_path, "rb")) == NULL){
                    error(1, "File not found.", sockfd);
                    perror("open()");
                }else{
                    int tries = 0, sz, n;
                    unsigned int packetNO = 1;
                    char data[512];

                    sz = fread(data, 1, 512, fd);
                    do{
                        sendPacket(3, packetNO, sockfd, data, sz);    
                        
                        if((n = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                            perror("select()");
                        }

                        while(n == 0){
                            sendPacket(3, packetNO, sockfd, data, sz);
                            if(tries == 10){
                                error(0, "Connection tiemout.", sockfd);
                            }
                            tries++;
                            if((n = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                                perror("select()");
                            }
                        }
                        tries = 0;

                        recvfrom(sockfd, message, sizeof(message) - 1, 0, (struct sockaddr *) &client, &len);

                        bit.bytes[0] = message[3];
                        bit.bytes[1] = message[2];
                      
                        if(bit.blocknumber == packetNO){
                            packetNO++;
                            sz = fread(data, 1, 512, fd);
                        } 
                    }while(sz > 0);
                    fclose(fd);
                }
            }else{
                if(OP == 2) {
                    error(0, "This operation is not supported!", sockfd);
                } else {
                    error(4, "Illegal TFTP operation.!", sockfd);
                }            
            }
        } else {
            printMsg("No message in five seconds."); 
        }
    }
    return 0;
}
void printMsg(char *print){
    fprintf(stdout, "%s\n", print);
    fflush(stdout);
}

void checkDir(DIR *dir, char *folder){
    if(!(dir = opendir(folder))){
        perror("opendir()");
        exit(0);
    }else{
        if(closedir(dir) != 0){
            perror("closedir()");
            exit(0);
        }
    }
}

void buildPath(char *folder, char *message){
    strcpy(file_path, folder);
    strcat(file_path, "/");
    strcat(file_path, &message[2]);
}

void error(int errorCode, const char *errorMsg, int sockfd){
    int n = 16 + strlen(errorMsg);
    char msg[n];
    msg[0] = 0;
    msg[1] = 5;
    msg[2] = 0;
    msg[3] = errorCode;
    int i;
    for(i = 4; i < n-1; i++){
        msg[i] = errorMsg[i - 4];
    }
    msg[n] = '\0'; 

    printf("%s\n", &msg[0]);
    fflush(stdout);   
        
    sendto(sockfd, msg, (size_t) n, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));
}

void sendPacket(int opCode, int blockNO, int sockfd, char *data, int size){
    int n = 4 + size;
    char msg[n];
    bit.blocknumber = htons(blockNO);
    msg[0] = 0;
    msg[1] = opCode;
    msg[2] = bit.bytes[0];
    msg[3] = bit.bytes[1];

    int i;
    for(i = 4; i < n; i++){
        msg[i] = data[i - 4];
    } 
    
    /* Send the message. */
    sendto(sockfd, msg, (size_t) n, 0, (struct sockaddr *) &client, (socklen_t) sizeof(client));
}

int argCheck(int port_n, char *folder) {

    if(!port_n){
        printMsg("Argument 1 not an integer");
        return 0;
    } /* LOL */
    else if(sizeof(folder) > 255){ 
        printMsg("Access violation.");
        return 0;
    } 
    return 1;
}
