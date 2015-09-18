#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

//User defined helper functions
void checkDir(DIR *dir, char *folder);
void buildPath(char *folder, char *message);
void error(int errorCode, const char *Errormsg, int sockfd);
void sendPacket(int opCode, int blockNO, int sockfd, char *data);
union { unsigned short blocknumber; char bytes[2]; } temp;

//global variables
char file_path[1];
struct sockaddr_in server, client;

int main(int argc, char **argv){
    int port_n, sockfd;
    char *folder;
    DIR *dir;
    char message[512];
    FILE *fd;
    
    //Check if the number of arguments is correct
    if(argc != 3){
        //Later change this to present a menu asking for destination n' stuff
        printf("Number of arguments incorrect.\n");
        return 0;
    }else{
        //need to think on how to defend agains buffer overflow
        folder = malloc(sizeof(&argv[2][0]));
        if(!sscanf(&argv[1][0], "%d", &port_n)){
            printf("Argument 1 not an integer.\n");
            return 0;
        }
        else if(!sscanf(&argv[2][0], "%s", folder)){
            printf("Argument 2 not a string.\n");
            return 0;
        }
    }

    //Check if directory is available to server
    checkDir(dir, folder);
    /* Create and bind a UDP socket */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){ /* SOCK_DGRAM  = using UDP */
        perror("cannot create socket");
        return 0;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    /* Network functions need arguments in network byte order instead of
       host byte order. The macros htonl, htons convert the values, */
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
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if((retval = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                perror("select()");
        } else if (retval > 0) {

                /* Data is available, receive it. */
                assert(FD_ISSET(sockfd, &rfds));
                
                /* Copy to len, since recvfrom may change it. */
                socklen_t len = (socklen_t) sizeof(client);
                /* Receive one byte less than declared,
                   because it will be zero-termianted
                   below. */

                ssize_t n = recvfrom(sockfd, message,
                                     sizeof(message) - 1, 0,
                                     (struct sockaddr *) &client,
                                     &len);

                 /* Zero terminate the message, otherwise
                   printf may access memory outside of the
                   string. */
                message[n] = '\0';

                //check OP code, only allow get
                int OP = message[1];
                if(OP == 1){
                    /*Build file path argument string and
                        open file*/
                    buildPath(folder, message);
                    if((fd = fopen(file_path, "r")) == NULL){
                        error(0, "File not there.", sockfd);
                        perror("open()");
                        exit(0);
                    }else{
                        int tries = 0, sz;
                        int packetNO = 1, awkPno = 0;
                        char data[512];

                        sz = fread(data, 1, 512, fd);
                        do{
                            sendPacket(3, packetNO, sockfd, data);
                            while(select(sockfd + 1, &rfds, NULL, NULL, &tv) == 0){
                                sendPacket(3, packetNO, sockfd, data);
                                if(tries == 10){
                                    error(0, "Connection tiemout.", sockfd);
                                    exit(0);
                                }
                                tries++;
                            }
                            recvfrom(sockfd, message,
                                 sizeof(message) - 1, 0,
                                 (struct sockaddr *) &client,
                                 &len);

                            awkPno = message[3];
                            if(awkPno == packetNO){
                                packetNO++;
                                sz = fread(data, 1, 512, fd);
                            }
                        }while(sz > 0);
                        shutdown(sockfd, SHUT_WR);
                        fclose(fd);
                        //close(sockfd);
                    } 
                } else{
                    // reject request
                    error(0, "This operation is not supported!", sockfd);
                    exit(0);
                }
        } else {
                fprintf(stdout, "No message in five seconds.\n"); 
                fflush(stdout);
        }
    }
    return 0;
}

void checkDir(DIR *dir, char *folder){
    if(!(dir = opendir(folder))){
        perror("opendir()");
        exit(0);
    }else{
        printf("Folder %s available.\n", folder);
        fflush(stdout);
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
    printf("file path: %s\n", file_path);
    fflush(stdout);
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
        
    sendto(sockfd, msg, (size_t) n, 0,
                       (struct sockaddr *) &client,
                       (socklen_t) sizeof(client));
}

void sendPacket(int opCode, int blockNO, int sockfd, char *data){
    int n = 4 + strlen(data);
    char msg[n];
    temp.blocknumber = htons(blockNO);
    msg[0] = 0;
    msg[1] = opCode;
    msg[2] = temp.bytes[0];
    msg[3] = temp.bytes[1];
    int i;
    for(i = 4; i < n; i++){
        msg[i] = data[i - 4];
    }

    /* Send the message. */
    sendto(sockfd, msg, (size_t) n, 0,
           (struct sockaddr *) &client,
           (socklen_t) sizeof(client));
               

}

