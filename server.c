
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <time.h>

#define PORT "5000"
#define MAX_NAME 50
#define MAX_DATA 100
#define MAX_CONNECTIONS 100
#define MAX_PACKETSIZE (MAX_NAME + MAX_DATA + 10)

#define USER_COUNT 3
const char * usernames[] = {
    "tesla",
    "maxwell",
    "ampere",
};
const char * passwords[] = {
    "123456",
    "qwerty",
    "trustno1",
};

char* format_data(unsigned type, unsigned size,
                  char *source, char *data) {
    char * msg_str = (char *) malloc (MAX_PACKETSIZE * sizeof(char));
    sprintf(msg_str,"%d:%d:%s:%s", type, size, source, data);
    return msg_str;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

typedef struct node
{
    bool activeSocket;
    char* username;
    char* joinID;
    clock_t sysTime;
} node;

int main(void)
{
    fd_set master, read_fds;
    int fdmax, listener, newfd, nbytes;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char buf[256];
    char remoteIP[INET6_ADDRSTRLEN];
    node connections[MAX_CONNECTIONS];
    for (int i = 0; i < MAX_CONNECTIONS; i++){
      connections[i].activeSocket = false;
    }

    int yes = 1;
    int i, j, rv;
    struct addrinfo hints, *ai, *p;
    char* list = (char *) malloc (MAX_PACKETSIZE * sizeof(char));

    FD_ZERO(&master);
    FD_ZERO(&read_fds);


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai);
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    FD_SET(listener, &master);
    fdmax = listener;

    while(true) {
        read_fds = master;
        struct timeval tv = {1, 0};
        if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
            perror("select");
            exit(4);
        }

        for(i = 0; i <= fdmax; i++) {
            if (i != listener && connections[i].activeSocket){
              clock_t t = time(NULL) - connections[i].sysTime;
              double time_taken = ((double)t); // in seconds
              //printf("%f\n", time_taken);
              if (time_taken > 60 * 5){
                unsigned rType = 13;
                unsigned rSize;
                char rSource[100] = " ";
                char rData[100] = " ";
                char* rPacket;

                strcpy(rData,
                  "You were idle for more than 5 minutes. Connection terminated.");
                rSize = strlen(rData);
                rPacket = format_data(rType, rSize,
                rSource, rData);
                if (send(i, rPacket, strlen(rPacket),
                 0) == -1) {
                    perror("send");
                };
                connections[i].activeSocket = false;
                close(i);
                FD_CLR(i, &master);
              }
            }

            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master);
                        if (newfd > fdmax) {
                            fdmax = newfd;
                        }
                        connections[newfd].activeSocket = true;
                        connections[newfd].sysTime = time(NULL);
                        connections[newfd].username =
                        (char *) malloc (MAX_PACKETSIZE * sizeof(char));
                        connections[newfd].joinID =
                        (char *) malloc (MAX_PACKETSIZE * sizeof(char));
                        strcpy(connections[newfd].joinID, "defaultSession");

                        printf("New connection from %s on "
                            "socket %d.\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    connections[i].sysTime = time(NULL);
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        if (nbytes == 0) {
                            connections[i].activeSocket = false;
                            printf("selectserver: socket %d hung up\n", i);
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            perror("recv");
                        }
                    } else {
                        buf[nbytes] = '\0';

                        char delim[] = ":";
                        unsigned type = atoi(strtok(buf,delim));
                        unsigned size = atoi(strtok(NULL, delim));
                        char *source = strtok(NULL, delim);
                        char *data = strtok(NULL, delim);

                        unsigned rType = -1;
                        unsigned rSize;
                        char rSource[100] = " ";
                        char rData[100] = " ";
                        char* rPacket;

                        bool valid = false;
                        switch (type){
                          case 0:
                            for (int k = 0; k < USER_COUNT; k++){
                              if (!strcmp(usernames[k], source) &&
                                  !strcmp(passwords[k], data)){
                                    strcpy(connections[i].username, source);
                                    rType = 1;
                                    rSize = strlen(rData);
                                    rPacket = format_data(rType, rSize,
                                    rSource, rData);
                                    if (send(i, rPacket, strlen(rPacket),
                                     0) == -1) {
                                        perror("send");
                                    };
                                    valid = true;
                                    break;
                                }
                            }

                            if (valid) break;

                            rType = 2;
                            strcpy(rData,
                              "Username and password combination does not match.");
                            rSize = strlen(rData);
                            rPacket = format_data(rType, rSize,
                            rSource, rData);
                            if (send(i, rPacket, strlen(rPacket),
                             0) == -1) {
                                perror("send");
                            };
                            connections[i].activeSocket = false;
                            close(i);
                            FD_CLR(i, &master);
                            break;

                          case 3:
                            strcpy(list,
                              "Username-Session\n");
                            char* pointer = list + strlen(list);
                            for (int ses = 0; ses < MAX_CONNECTIONS; ses++){
                              if (!connections[ses].activeSocket) continue;

                              strcpy(pointer, connections[ses].username);
                              pointer = pointer + strlen(pointer);
                              *pointer = '-';
                              pointer++;

                              strcpy(pointer, connections[ses].joinID);
                              pointer = pointer + strlen(pointer);
                              strcpy(pointer, "\n");
                              pointer = pointer + strlen(pointer);
                            }
                            pointer++;
                            *pointer = '\0';

                            rType = 12;
                            strcpy(rData, list);
                            rSize = strlen(rData);
                            rPacket = format_data(rType, rSize,
                            rSource, rData);
                            if (send(i, rPacket, strlen(rPacket),
                             0) == -1) {
                                perror("send");
                            };
                          break;

                          case 4:
                            //join session
                            for (int ses = 0; ses < MAX_CONNECTIONS; ses++){
                              if (connections[ses].activeSocket &&
                                !strcmp(connections[ses].joinID, data)){
                                  strcpy(connections[i].joinID, data);
                                  rType = 5;
                                  strcpy(rData, data);
                                  rSize = strlen(rData);
                                  rPacket = format_data(rType, rSize,
                                  rSource, rData);
                                  if (send(i, rPacket, strlen(rPacket),
                                   0) == -1) {
                                      perror("send");
                                  };
                                  break;
                              }
                            }

                            if (rType == 5) break;
                            rType = 6;
                            strcpy(rData, data);
                            rSize = strlen(rData);
                            rPacket = format_data(rType, rSize,
                            rSource, rData);
                            if (send(i, rPacket, strlen(rPacket),
                             0) == -1) {
                                perror("send");
                            };
                            break;
                          case 7:
                            strcpy(connections[i].joinID, "defaultSession");
                            break;

                          case 8:
                            rType = 9;

                            strcpy(rData, data);
                            strcpy(connections[i].joinID, data);
                            strcpy(connections[i].username, source);
                            rSize = strlen(rData);
                            rPacket = format_data(rType, rSize,
                            rSource, rData);
                            if (send(i, rPacket, strlen(rPacket),
                             0) == -1) {
                                perror("send");
                            };
                            break;

                          case 10:
                            rType = 10;
                            strcpy(rData, data);
                            rSize = strlen(rData);
                            strcpy(rSource,source);
                            rPacket = format_data(rType, rSize,
                            rSource, rData);

                            bool usernameExists = false;
                            for (j = 0; j <= fdmax; j++){
                              if (FD_ISSET(j, &master)) {
                                  if (j != listener && j != i
                                    && !strcmp(connections[i].joinID,
                                      connections[j].username)) {
                                      usernameExists = true;
                                      if (send(j, rPacket, strlen(rPacket),
                                       0) == -1) {
                                          perror("send");
                                      }
                                  }
                              }
                            }

                            if (usernameExists) break;

                            for(j = 0; j <= fdmax; j++) {
                                if (FD_ISSET(j, &master)) {
                                    if (j != listener && j != i
                                      && !strcmp(connections[i].joinID,
                                        connections[j].joinID)) {

                                        if (send(j, rPacket, strlen(rPacket),
                                         0) == -1) {
                                            perror("send");
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    return 0;
}
