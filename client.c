#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>

#define DATABUFFERSIZE (20 << 20)

int recv_code(int control_sock, char* recvbuffer, uint size) {
    int recvlen = 0;
    int trynum = 0;
    while (1) {
        recvlen = read(control_sock, recvbuffer, size);
        recvbuffer[recvlen] = 0;
        if (recvlen > 0) {
            printf("[Server]: %s", recvbuffer);
            int code = 0;
            sscanf(recvbuffer, "%d", &code);
            return code;
        }
        else {
            ++trynum;
            if (trynum >= 5) {
                errx(1, "ERROR: Connection not available.\n");
            }
            printf("Nothing received, waiting to try %d time(s).\n", trynum);
            sleep(3);
        }
    }
}

int estab_data_sock(int control_sock, char* serveraddr) {
    struct sockaddr_in server;
    memset(&server, 0, sizeof(struct sockaddr_in));
    char recvbuffer[1024];
    memset(recvbuffer, 0, sizeof(recvbuffer));
    char sendbuffer[1024];
    memset(sendbuffer, 0, sizeof(sendbuffer));
    sprintf(sendbuffer, "PASV\r\n");
    write(control_sock, sendbuffer, strlen(sendbuffer));
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
    uint dataport = 0;
    uint dataporttemp[7] = {0};
    char* sta = strstr(recvbuffer, "Entering Passive Mode (");
    while (sta == NULL) {
        recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
        sta = strstr(recvbuffer, "Entering Passive Mode (");
    }
    sscanf(sta, "Entering Passive Mode (%u,%u,%u,%u,%u,%u).", &dataporttemp[1], &dataporttemp[2], &dataporttemp[3], &dataporttemp[4], &dataporttemp[5], &dataporttemp[6]);
    dataport = (dataporttemp[5] << 8) + dataporttemp[6];
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    inet_pton(AF_INET, serveraddr, &server.sin_addr);
    server.sin_port = htons(dataport);
    connect(data_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
    return data_sock;
}

void write_data(char* databuffer, int databuffersize, int data_sock, FILE* stream) {
    memset(databuffer, 0, databuffersize);
    while (1) {
        int blocksize = read(data_sock, databuffer, databuffersize);
        if (blocksize == 0)
            break;
        fwrite(databuffer, sizeof(char), blocksize, stream);
    }
}

int main() {
    int control_sock = 0;
    struct sockaddr_in server;
    memset(&server, 0, sizeof(struct sockaddr_in));
    char serveraddr[1024];
    memset(serveraddr, 0, sizeof(serveraddr));
    char recvbuffer[1024];
    memset(recvbuffer, 0, sizeof(recvbuffer));
    char sendbuffer[1024];
    memset(sendbuffer, 0, sizeof(sendbuffer));
    unsigned int port = 0;
    char* databuffer = (char*)malloc(sizeof(char) * DATABUFFERSIZE);
    memset(databuffer, 0, sizeof(char) * DATABUFFERSIZE);


    printf("[Client]: IP Address: ");
    scanf("%s", serveraddr);
    printf("[Client]: Port: ");
    scanf("%u", &port);
    printf("[Client]: Server IP: %s    Server Port: %u    Link start!\n", serveraddr, port);

    server.sin_family = AF_INET;
    inet_pton(AF_INET, serveraddr, &server.sin_addr);
    server.sin_port = htons(port);

    //Establish connect
    control_sock = socket(AF_INET, SOCK_STREAM, 0);
    connect(control_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));

    //Login Test(ftptester 123456)
    //username
    printf("[Client]: Name: ");
    char username[1024];
    memset(username, 0, sizeof(username));
    scanf("%s", username);
    sprintf(sendbuffer, "USER %s\r\n", username);
    write(control_sock, sendbuffer, strlen(sendbuffer));
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
    //passwd
    printf("[Client]: Password: ");
    char passwd[1024];
    memset(passwd, 0, sizeof(passwd));
    scanf("%s", passwd);
    getchar();
    sprintf(sendbuffer, "PASS %s\r\n", passwd);
    write(control_sock, sendbuffer, strlen(sendbuffer));
    if (recv_code(control_sock, recvbuffer, sizeof(recvbuffer)) != 230)
        return 1;

    //int data_sock = estab_data_sock(control_sock, serveraddr);

    char order[1024];
    memset(order, 0, sizeof(order));
    printf("[Client]: ");
    fgets(order, 1024, stdin);
    order[strlen(order) - 1] = 0;
    while (1) {
        if (order == strstr(order, "get ")) { //download
            char filename[1024] = {0};
            char localfilename[1024] = {0};
            int fnnum = sscanf(order, "get %s %s", filename, localfilename);
            if (fnnum == 2) {
                sprintf(sendbuffer, "SIZE %s\r\n", filename);
                write(control_sock, sendbuffer, strlen(sendbuffer));

                int rcode = recv_code(control_sock, recvbuffer, sizeof(recvbuffer));

                if (rcode == 213) {
                    int filesize = 0;
                    sscanf(recvbuffer, "%d%d", &rcode, &filesize);
                    int leftsize = filesize;
                    FILE* file_handle = fopen(localfilename, "wb");
                    if (file_handle) {
                        int data_sock = estab_data_sock(control_sock, serveraddr);
                        sprintf(sendbuffer, "RETR %s\r\n", filename);
                        write(control_sock, sendbuffer, strlen(sendbuffer));
                        rcode = recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
                        if (rcode == 150) {
                            write_data(databuffer, DATABUFFERSIZE, data_sock, file_handle);
                            printf("finish\n");
                            fclose(file_handle);
                            recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
                        }
                        close(data_sock);
                    }
                    else {
                        printf("Fail to create file!\n");
                    }
                }
            }
            else {
                printf("Usage: get remote-file local-file\n");
            }
        }
        else if (order == strstr(order, "put ")) {
            char filename[1024] = {0};
            char remotename[1024] = {0};
            int fnnum = sscanf(order, "put %s %s", filename, remotename);
            if (fnnum == 2) {
                FILE* file_handle = fopen(filename, "rb");
                if (file_handle) {
                    int data_sock = estab_data_sock(control_sock, serveraddr);
                    sprintf(sendbuffer, "STOR %s\r\n", remotename);
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                    int rcode = recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
                    if (rcode == 150) {
                        memset(databuffer, 0, DATABUFFERSIZE);
                        while (1) {
                            int blocksize = fread(databuffer, sizeof(char), DATABUFFERSIZE, file_handle);
                            if (blocksize == 0)
                            break;
                            write(data_sock, databuffer, blocksize);
                        }
                        close(data_sock);
                        fclose(file_handle);
                        recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
                    }
                    else {
                        close(data_sock);
                        fclose(file_handle);
                    }
                }
                else {
                    printf("No such file!\n");
                }
            }
            else {
                printf("Usage: put localfile remotefile\n");
            }
        }
        else if (strcmp(order, "pwd") == 0) {
            sprintf(sendbuffer, "PWD\r\n");
            write(control_sock, sendbuffer, strlen(sendbuffer));
            recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
        }
        else if (strcmp(order, "dir") == 0) {
            int data_sock = estab_data_sock(control_sock, serveraddr);
            sprintf(sendbuffer, "LIST\r\n");
            write(control_sock, sendbuffer, strlen(sendbuffer));
            int rcode = recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
            if (rcode == 150) {
                write_data(databuffer, DATABUFFERSIZE, data_sock, stdout);
                recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
            }
            close(data_sock);
        }
        else if (order == strstr(order, "cd ")) {
            char pathname[1024] = {0};
            strcpy(pathname, &order[3]);
            sprintf(sendbuffer, "CWD %s\r\n", pathname);
            write(control_sock, sendbuffer, strlen(sendbuffer));
            recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
        }
        else if (strcmp(order, "?") == 0) {
            printf("Usage:\nget: download one remote file;\nput: upload one remote file;\npwd: present working directory;\ndir: list of files in the remote directory;\ncd: change remote directory;\n?: help;\nquit: exit;\n");
        }
        else if (strcmp(order, "quit") == 0) {
            break;
        }
        else {
            printf("Illegal order!\nUsage:\nget: download one remote file;\nput: upload one remote file;\npwd: present working directory;\ndir: list of files in the remote directory;\ncd: change remote directory;\n?: help;\nquit: exit;\n");
        }
        printf("[Client]: ");
        fgets(order, 1024, stdin);
        order[strlen(order) - 1] = 0;
    }
    //quit
    free(databuffer);
    sprintf(sendbuffer, "QUIT\r\n");
    write(control_sock, sendbuffer, strlen(sendbuffer));
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
    close(control_sock);

    return 0;
}
