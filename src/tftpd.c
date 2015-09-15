#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv){
    int port_n;
    char *file_path;
    int sockfd;
    struct sockaddr_in server, client;
    char message[512];
    //int srcfd, destfd;

    //Check if the number of arguments is correct
    if(argc != 3){
        //Later change this to present a menu asking for destination n' stuff
        printf("Number of arguments incorrect.\n");
        return 0;
    }else{
        //need to think on how to defend agains buffer overflow
        file_path = malloc(sizeof(&argv[2][0]));
        if(!sscanf(&argv[1][0], "%d", &port_n)){
            printf("Argument 1 not an integer.\n");
            return 0;
        }
        else if(!sscanf(&argv[2][0], "%s", file_path)){
            printf("Argument 2 not a string.\n");
            return 0;
        }
    }

    /* Create and bind a UDP socket */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
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
    fprintf(stdout, "Server is ready and listening on port %d\n", ntohs(server.sin_port));
    fflush(stdout);

    /*if((srcfd = open(file_path, O_RDONLY)) == -1){
        perror("open failed");
        return -1;
    }*/
    //if((destfd = open()))
    while(1){
        fd_set rfds;
        struct timeval tv;
        int retval;

        /* Check whether there is data on the socket fd. */
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        /* Wait for five seconds. */
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
                /* Send the message back. */
                sendto(sockfd, message, (size_t) n, 0,
                       (struct sockaddr *) &client,
                       (socklen_t) sizeof(client));
                /* Zero terminate the message, otherwise
                   printf may access memory outside of the
                   string. */
                message[n] = '\0';
                /* Print the message to stdout and flush. */
                fprintf(stdout, "Received:\n%s\n", message);
                fflush(stdout);
        } else {
                fprintf(stdout, "No message in five seconds.\n");
                fflush(stdout);
        }
    }
    //infinity loop
    //opna file
    //lese file í buffer
    //senda file til client, passa uppá header

    return 0;
}
