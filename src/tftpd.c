#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv){
    int port_n;
    char **file_path;
    //Check if the number of arguments is correct
    if(argc != 3){
        //Later change this to present a menu asking for destination n' stuff
        printf("Number of arguments incorrect.\n");
    }

    port_n = argv[1];
    file_path = argv[2];


    return 0;
}
