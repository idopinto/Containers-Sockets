//
// Created by ilana315061945 on 10/06/2022.
//

#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <cstring>
#include <netdb.h>
#include <cstdio>
#include <string>

#define MAX_CLIENTS 5
#define MAXHOSTNAME 256
#define BUFFER_SIZE 256

#define SYS_ERR "system error:"
#define WRITE_ERR  "write failed..."

#define SPRINTF_ERR "sprintf failed..."

#define CALL_SOCKET_ERR "creating client failed..."

#define ARGV_ERR "invalid command line arguments..."

#define GETHOSTNAME_ERR "gethostname function failed..."

#define GET_CONNECTION_ERR "get connection failed..."

#define ESTABLISH_ERR " server establish failed... "

#define LISTEN_ERR "listen function failed... "

#define READ_DATA_ERR "read_data function failed..."

#define SYSTEM_ERR "system function failed..."

/**
 * this function prints the error massages and exit the program
 * @param fmt the massage
 */
void error_massage(const char *fmt){
    fprintf (stderr,"%s %s\n",SYS_ERR,fmt);
    exit (1);
}


/**
 * establish the server socket
 */
int establish_server_socket(unsigned short portnum) {
    char myname[MAXHOSTNAME+1];
    int server_socket;
    struct sockaddr_in server_address;
    struct hostent *hp;


    //hostnet initialization
    if(gethostname(myname, MAXHOSTNAME)<0){
        return (-1);
    }
    hp = gethostbyname(myname);
    if (hp == NULL)
        return(-1);

    //sockaddrr_in initlization
    bzero(&server_address, sizeof (server_address));

    server_address.sin_family = hp->h_addrtype;

    /* this is our host address */
    memcpy(&server_address.sin_addr, hp->h_addr, hp->h_length);

    /* this is our port number */
    server_address.sin_port= htons(portnum);

    /* create socket */
    if ((server_socket= socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return(-1);

    if (bind(server_socket , (struct sockaddr *)&server_address , sizeof(struct sockaddr_in)) < 0) {
        close(server_socket);
        return(-1);
    }

    if(listen(server_socket, MAX_CLIENTS)<0){error_massage(LISTEN_ERR);}
    return server_socket;
}

/**
 * this function waits for calls
 * and return a new socket which is connected to the caller
 */
int get_connection(int s) {
    int new_socket; /* socket of connection */
    if ((new_socket = accept(s,NULL,NULL)) < 0)
        return -1;
    return new_socket;
}
/**
 * creating the client socket
 */
int call_socket(char *hostname, unsigned short portnum) {

    struct sockaddr_in server_address;
    struct hostent *hp;
    int client_socket;

    if ((hp= gethostbyname (hostname)) == NULL) {
        return(-1);
    }

    memset(&server_address, 0, sizeof(server_address));
    memcpy((char *)&server_address.sin_addr , hp->h_addr ,
           hp->h_length);
    server_address.sin_family = hp->h_addrtype;
    server_address.sin_port = htons((u_short)portnum);

    if ((client_socket = socket(hp->h_addrtype,SOCK_STREAM, 0)) < 0) {
        return(-1);
    }
    if (connect(client_socket, (struct sockaddr *)&server_address , sizeof(server_address)) < 0) {
        close(client_socket);
        return(-1);
    }

    return client_socket;
}
/**
 * this function reading the data from the socket and put it in the buf
 */
int read_data(int server_socket, char *buf, int buffer_length) {
    int bcount = 0;       /* counts bytes read */
    int br = 0;            /* bytes read this pass */
    while (bcount < buffer_length) { /* loop until full buffer */
        br = read(server_socket, buf, buffer_length - bcount);
        if (br > 0){
            bcount += br;
            buf += br;
        }

        if (br == 0 ) {
            break;

        }
        if(br == -1){
            error_massage(READ_DATA_ERR);
        }

    }
    return bcount;
}

int main(int argc, char* argv[]){
    char command_to_execute[BUFFER_SIZE];  // to send
    char recv_command_to_execute[BUFFER_SIZE];  // receive
    int command_length;
    int server_fd,new_socket_fd;
    char hostname[MAXHOSTNAME + 1];
    if(argc < 3){
        error_massage(ARGV_ERR);
    }
    if(argc == 3){
        if(strcmp(argv[1],"server") != 0){
            error_massage(ARGV_ERR);
        }
        // server side
        unsigned int server_port_num = (strtoul(argv[2], nullptr, 10));
        server_fd = establish_server_socket(server_port_num);
        if(server_fd == -1){
            error_massage(ESTABLISH_ERR);
        }
        while(true){
            new_socket_fd = get_connection(server_fd);
            if(new_socket_fd == -1){
                error_massage(GET_CONNECTION_ERR);
            }
            read_data(new_socket_fd,recv_command_to_execute,BUFFER_SIZE);
            // run the command that client sent
            if(system(recv_command_to_execute)<0){
                error_massage(SYSTEM_ERR);
            };
            close(new_socket_fd);
            bzero(recv_command_to_execute, strlen(recv_command_to_execute));
        }

    }
    else if (argc >= 4){
        if(strcmp(argv[1],"client") != 0){
            error_massage(ARGV_ERR);
        }
        // client side
        if(gethostname(hostname, MAXHOSTNAME)<0){
            error_massage(GETHOSTNAME_ERR);
        }
        unsigned int client_port_num = (strtoul(argv[2],nullptr,10));
        int client_socket = call_socket(hostname, client_port_num);
        if(client_socket == -1){
            error_massage(CALL_SOCKET_ERR);
        }

        // send argv[2]
        int counter = 3;
        std::string command;
        while(counter < argc){
            command += argv[counter];
            counter++;
            if(counter != argc){
                command += " ";
            }
        }
        if(sprintf(command_to_execute,"%s", const_cast<char*>(command.c_str()))<0){ error_massage(SPRINTF_ERR);}
        command_length = (int) strlen(command_to_execute);
        if(write(client_socket,command_to_execute,command_length)!=command_length){
            error_massage(WRITE_ERR);
        }
    }
}