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
    }else{
        sscanf(argv[1], '%d', &port_n);  
        sscanf(&argv[2], '%s', &file_path);
        printf("%d %s\n", port_n, file_path);
    }
    return 0;

}
