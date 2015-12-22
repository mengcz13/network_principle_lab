#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define DATABUFFERSIZE (20 << 20)
#define FTPPORT 21212

typedef enum{USER, PASS, PASV, SIZE, RETR, STOR, PWD, LIST, CWD, QUIT}ORDER;

void recv_code(int control_sock, char* recvbuffer, unsigned int size) {
    int recvlen = 0;
    int trynum = 0;
    while (1) {
        recvlen = read(control_sock, recvbuffer, size);
        recvbuffer[recvlen] = 0;
        if (recvlen > 0) {
            printf("[Client]: %s", recvbuffer);
            int code = 0;
            sscanf(recvbuffer, "%d", &code);
            return;
        }
        else {
            ++trynum;
            if (trynum >= 5) {
                printf("ERROR: Connection not available.\n");
                pthread_exit(NULL);
                return;
            }
            printf("Nothing received, waiting to try %d time(s).\n", trynum);
            sleep(3);
        }
    }
}

void get_new_path(char* wd, char* nd) {
    if (nd[0] == '/') { //absolute path
        strcpy(wd, nd);
    }
    else if (nd[0] == '~') {
        strcpy(wd, nd);
    }
    else if (strcmp(nd, "../") == 0) {
        if (strcmp(wd, "/") == 0)
            return;
        else {
            int i = 0;
            for (i = strlen(wd) - 2; i >=0; --i) {
                if (wd[i] == '/') {
                    wd[i + 1] = 0;
                    return;
                }
            }
        }
    }
    else if (strcmp(nd, "./") == 0) {
        return;
    }
    else {
        strcat(wd, nd);
    }
}

void make_file_path(char* des, char* wd, char* filename) {
    strcpy(des, wd);
    get_new_path(des, filename);
}

void* communicate(void* ctrl_sock) {
    int control_sock = *((int*)ctrl_sock);

    struct sockaddr_in data_listen_addr, client_data_addr;
    int sockaddrinlen = sizeof(data_listen_addr);
    memset(&data_listen_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_data_addr, 0, sizeof(struct sockaddr_in));
    int data_listen_sock = 0, data_sock = 0;
    char recvbuffer[1024];
    memset(recvbuffer, 0, sizeof(recvbuffer));
    char sendbuffer[1024];
    memset(sendbuffer, 0, sizeof(sendbuffer));
    char* databuffer = (char*)malloc(sizeof(char) * DATABUFFERSIZE);
    memset(databuffer, 0, sizeof(char) * DATABUFFERSIZE);

    char working_dir[1024] = "/home/mengcz/ftpserver/";

    sprintf(sendbuffer, "220 Hello\r\n");
    write(control_sock, sendbuffer, strlen(sendbuffer));

    char username[1024] = {0};
    char passwd[1024] = {0};
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
    sscanf(recvbuffer, "USER %s\r\n", username);
    sprintf(sendbuffer, "331 Please specify the password.\r\n");
    write(control_sock, sendbuffer, strlen(sendbuffer));
    recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
    sscanf(recvbuffer, "PASS %s\r\n", passwd);
    if (1) {
        sprintf(sendbuffer, "230 Login successful.\r\n");
        write(control_sock, sendbuffer, strlen(sendbuffer));
        while (1) {
            recv_code(control_sock, recvbuffer, sizeof(recvbuffer));
            if (strcmp(recvbuffer, "PASV\r\n") == 0) {
                data_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
                int templen = sizeof(data_listen_addr);
                data_listen_addr.sin_family = AF_INET;
                data_listen_addr.sin_addr.s_addr = INADDR_ANY;
                bind(data_listen_sock, (struct sockaddr*)&data_listen_addr, sizeof(data_listen_addr));
                listen(data_listen_sock, 5);
                getsockname(data_listen_sock, (struct sockaddr*)&data_listen_addr, (socklen_t*)&templen);
                int portnum = ntohs(data_listen_addr.sin_port);
                sprintf(sendbuffer, "227 Entering Passive Mode (0,0,0,0,%d,%d).\r\n", (portnum >> 8), (portnum & ((1 << 8) - 1)));
                write(control_sock, sendbuffer, strlen(sendbuffer));
                data_sock = accept(data_listen_sock, (struct sockaddr*)&client_data_addr, (socklen_t*)&sockaddrinlen);
            }
            else if (recvbuffer == strstr(recvbuffer, "POST ")) {
                sprintf(sendbuffer, "Please use passive mode.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
            else if (recvbuffer == strstr(recvbuffer, "SIZE ")) {
                char filepath[1024] = {0};
                char filename[1024] = {0};
                sscanf(recvbuffer, "SIZE %s\r\n", filename);
                make_file_path(filepath, working_dir, filename);
                struct stat statbuff;
                if (stat(filepath, &statbuff) < 0) {
                    sprintf(sendbuffer, "550 Could not get file size.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                }
                else {
                    sprintf(sendbuffer, "213 %u\r\n", (unsigned int)statbuff.st_size);
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                }
            }
            else if (recvbuffer == strstr(recvbuffer, "RETR ")) {
                char filename[1024] = {0};
                char filepath[1024] = {0};
                struct stat statbuff;
                sscanf(recvbuffer, "RETR %s\r\n", filename);
                make_file_path(filepath, working_dir, filename);
                FILE* file_handle = fopen(filepath, "rb");
                if (file_handle) {
                    stat(filepath, &statbuff);
                    sprintf(sendbuffer, "150 Opening BINARY mode data connection for %s (%u bytes).\r\n", filepath, (unsigned int)statbuff.st_size);
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                    memset(databuffer, 0, DATABUFFERSIZE);
                    while (1) {
                        int blocksize = fread(databuffer, sizeof(char), DATABUFFERSIZE, file_handle);
                        write(data_sock, databuffer, blocksize);
                        if (blocksize == 0) {
                            sprintf(sendbuffer, "226 Transfer complete.\r\n");
                            write(control_sock, sendbuffer, strlen(sendbuffer));
                            printf("finish\n");
                            close(data_sock);
                            break;
                        }
                    }
                }
                else {
                    sprintf(sendbuffer, "550 Could not open the file.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                }
            }
            else if (recvbuffer == strstr(recvbuffer, "STOR ")) {
                char filename[1024] = {0};
                char filepath[1024] = {0};
                sscanf(recvbuffer, "STOR %s\r\n", filename);
                make_file_path(filepath, working_dir, filename);
                FILE* file_handle = fopen(filepath, "wb");
                if (file_handle) {
                    sprintf(sendbuffer, "150 OK to send data.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                    memset(databuffer, 0, DATABUFFERSIZE);
                    while (1) {
                        int blocksize = read(data_sock, databuffer, DATABUFFERSIZE);
                        if (blocksize == 0) {
                            sprintf(sendbuffer, "226 Transfer complete.\r\n");
                            write(control_sock, sendbuffer, strlen(sendbuffer));
                            close(data_sock);
                            fclose(file_handle);
                            break;
                        }
                        fwrite(databuffer, sizeof(char), blocksize, file_handle);
                    }
                }
                else {
                    sprintf(sendbuffer, "550 Could not open the file.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                }
            }
            else if (strcmp(recvbuffer, "PWD\r\n") == 0) {
                sprintf(sendbuffer, "257 \"%s\".\r\n", working_dir);
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
            else if (strcmp(recvbuffer, "LIST\r\n") == 0) {
                char sysorder[1024] = {0};
                sprintf(sysorder, "ls %s -l", working_dir);
                FILE* sysout = (FILE*)popen(sysorder, "r");
                if (sysout) {
                    sprintf(sendbuffer, "150 Here comes the directory listing.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                    memset(databuffer, 0, DATABUFFERSIZE);
                    while (1) {
                        int blocksize = fread(databuffer, sizeof(char), DATABUFFERSIZE, sysout);
                        write(data_sock, databuffer, blocksize);
                        if (blocksize == 0) {
                            sprintf(sendbuffer, "226 Directory send OK.\r\n");
                            write(control_sock, sendbuffer, strlen(sendbuffer));
                            close(data_sock);
                            break;
                        }
                    }
                }
                else {
                    sprintf(sendbuffer, "550 Could not open the directory.\r\n");
                    write(control_sock, sendbuffer, strlen(sendbuffer));
                }
            }
            else if (recvbuffer == strstr(recvbuffer, "CWD ")) {
                char newpathname[1024] = {0};
                sscanf(recvbuffer, "CWD %s\r\n", newpathname);
                int last = strlen(newpathname) - 1;
                if (newpathname[last] != '/') {
                    newpathname[last + 1] = '/';
                    newpathname[last + 2] = 0;
                }
                get_new_path(working_dir, newpathname);
                sprintf(sendbuffer, "Directory successfully changed.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
            else if (strcmp(recvbuffer, "QUIT\r\n") == 0) {
                sprintf(sendbuffer, "221 Goodbye.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
                close(control_sock);
                break;
            }
            else if (strcmp(recvbuffer, "SYST\r\n") == 0) {
                sprintf(sendbuffer, "NO SYST INFO.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
            else if (strcmp(recvbuffer, "TYPE I\r\n") == 0) {
                sprintf(sendbuffer, "200 Switching to Binary mode.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
            else if (strcmp(recvbuffer, "TYPE A\r\n") == 0) {
                sprintf(sendbuffer, "200 Switching to ASCII mode.\r\n");
                write(control_sock, sendbuffer, strlen(sendbuffer));
            }
        }

    }
    else {
        sprintf(sendbuffer, "530 Login incorrect.\r\n");
        write(control_sock, sendbuffer, strlen(sendbuffer));
    }

    free(databuffer);
    pthread_exit(NULL);
}

void* listen_and_accept_new_client(void* client_list) {
    int listen_sock = 0, control_sock = 0;
    struct sockaddr_in listen_addr, client_addr;
    int sockaddrinlen = sizeof(listen_addr);
    memset(&listen_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(FTPPORT);
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listen_sock, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) != 0) {
        printf("Cannot bind!\n");
        pthread_exit(NULL);
    }
    listen(listen_sock, 5);
    while (1) {
        pthread_t thnum;
        control_sock = accept(listen_sock, (struct sockaddr*)&client_addr, (socklen_t*)&sockaddrinlen);//modify to multithread: pass the control_sock to every thread
        int err = pthread_create(&thnum, NULL, communicate, &control_sock);
    }
    pthread_exit(NULL);
} 

int main() {
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    pthread_t thnum;
    int err = pthread_create(&thnum, NULL, listen_and_accept_new_client, NULL);
    char mainorder[1024];
    while (1) {
        scanf("%s", mainorder);
        if (strcmp(mainorder, "quit") == 0) {
            break;
        }
    }
    return 0;
}