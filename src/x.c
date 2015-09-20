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
void checkDir(DIR *dir, char *folder);
void buildPath(char *folder, char *message);
void error(int errorCode, const char *Errormsg, int sockfd);
void sendPacket(int opCode, int blockNO, int sockfd, char *data, int sizesz);

union { unsigned short blocknumber; char bytes[2]; } temp;

//global variables
char file_path[1];
struct sockaddr_in server, client;

int main(int argc, char **argv){
    int port_n, sockfd;
    char *folder;
    DIR *dir = NULL;
    
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
        int retval, pid;

        /* Check whether there is data on the socket fd. */
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        /* Set socket to wait for five seconds. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        if((retval = select(sockfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                perror("select()");
        } else if (retval > 0) {
            //fork to have child process handle requests
            //while parent listens for more requests
            pid = fork();
            if(pid == 0){
                //variables that each child has to have for itself
                struct sockaddr_in client;
                char message[512];
                FILE *fd;
                int childSocketfd;

                 /* Data is available, receive it. */
                assert(FD_ISSET(sockfd, &rfds));
                
                /* Copy to len, since recvfrom may change it. */
                socklen_t len = (socklen_t) sizeof(client);

                if((childSocketfd = accept(sockfd, (struct sockaddr *) &client,
                             &len)) == -1){
                    perror("accept");
                    exit(0);
                }

                close(sockfd); // child doesn't need the listener

                /* Receive one byte less than declared,
                   because it will be zero-termianted
                   below. */
                recvfrom(childSocketfd, message,
                             sizeof(message) - 1, 0,
                             (struct sockaddr *) &client,
                             &len);

                //////////////////////////////////////////
                /************ extracting mode ***********/
                /////////////////////////////////////////
                /*
                int i;
                char *mode;
                for(i = 2; i < (int)sizeof(message); i++){
                    if(message[i] == 0){
                        mode = &message[i + 1];
                        break; 
                    }
                }
                printf(">>>>>>>> %s\n", mode);
                */
               //////////////////////////////////////////

                //check OP code, only allow get
                int OP = message[1];
                printf("OPcode %d\n", OP);
                if(OP == 1){
                         printf("file %s requested by %s:%d\n", &message[2], inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                        
                        /*Build file path argument string and
                            open file*/
                        buildPath(folder, message);
                        if((fd = fopen(file_path, "r")) == NULL){
                            error(0, "File not there.", childSocketfd);
                            perror("open()");
                            exit(0);
                        }else{
                            int tries = 0, sz, n;
                            unsigned int packetNO = 1;
                            char data[512];

                            sz = fread(data, 1, 512, fd);
                            do{
                                if(sz < 512){   //handle last data packet
                                    char buf[sz + 1];
                                    memset(buf, 0, sizeof(buf));
                                    int k;
                                for(k = 0; k <= sz; k++){
                                    buf[k] = data[k];
                                }
                                    sendPacket(3, packetNO, childSocketfd, buf, sz);
                                }else{
                                    sendPacket(3, packetNO, childSocketfd, data, sz);    
                                }

                                if((n = select(childSocketfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                                        perror("select()");
                                        shutdown(childSocketfd, SHUT_WR);
                                        fclose(fd);
                                        exit(0);
                                    }
                                while(n == 0){
                                    sendPacket(3, packetNO, childSocketfd, data, sz);
                                    if(tries == 10){
                                        error(0, "Connection tiemout.", childSocketfd);
                                        shutdown(childSocketfd, SHUT_WR);
                                        fclose(fd);
                                        exit(0);
                                    }
                                    tries++;
                                    if((n = select(childSocketfd + 1, &rfds, NULL, NULL, &tv)) == -1){
                                        perror("select()");
                                        shutdown(childSocketfd, SHUT_WR);
                                        fclose(fd);
                                        exit(0);
                                    }
                                }
                                tries = 0;

                                recvfrom(childSocketfd, message,
                                     sizeof(message) - 1, 0,
                                     (struct sockaddr *) &client,
                                     &len);

                                temp.bytes[0] = message[3];
                                temp.bytes[1] = message[2];
                                if(temp.blocknumber == packetNO){
                                    packetNO++;
                                    sz = fread(data, 1, 512, fd);
                                } 
                            }while(sz > 0);
                            //shutdown(childSocketfd, SHUT_WR);
                            fclose(fd);
                            exit(0);
                        }
                   
                } else{
                    // reject request
                    error(0, "This operation is not supported!", childSocketfd);
                    exit(0);
                }
            }
        } else if (pid > 0){
            //parent procces
            //sleep(20);
            fprintf(stdout, "Server is listening.\n"); 
            fflush(stdout);
        }else{
            perror("fork()");
        }
    }
    return 0;
}

void checkDir(DIR *dir, char *folder){
    if(!(dir = opendir(folder))){
        perror("opendir()");
        exit(0);
    }else{
        /*printf("Folder %s available.\n", folder);
        fflush(stdout);*/
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
    /*printf("file path: %s\n", file_path);
    fflush(stdout);*/
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

void sendPacket(int opCode, int blockNO, int sockfd, char *data, int sz){
    int n = 4 + sz;
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
