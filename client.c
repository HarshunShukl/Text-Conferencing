#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <stdbool.h>

#define MAX_NAME 50
#define MAX_DATA 100
#define MAX_PACKETSIZE (MAX_NAME + MAX_DATA + 10)

bool handlePacket(char *msg) {
    char delim[] = ":";
    unsigned type = atoi(strtok(msg,delim));
    unsigned size = atoi(strtok(NULL, delim));
    char *source = strtok(NULL, delim);
    char *data = strtok(NULL, delim);

    switch (type){
      case 1:
        printf("Successful login.\n");
        return true;
        break;
      case 2:
        printf("Failed login. %s\n", data);
        return false;
        break;
      case 5:
        printf("Joined session %s.\n", data);
        return true;
        break;
      case 6:
        printf("Failed to join session. %s\n", data);
        return false;
        break;
      case 9:
        printf("Created session %s.\n", data);
        return false;
        break;
      case 10:
        printf("%s: %s\n", source, data);
        return false;
        break;
      case 12:
        printf("%s\n", data);
        return false;
        break;
      case 13:
        printf("%s\n", data);
        return false;
        break;
    }

    return false;
}


char* format_data(unsigned type, unsigned size,
                  char *source, char *data) {
    char * msg_str = (char *) malloc (MAX_PACKETSIZE * sizeof(char));
    sprintf(msg_str,"%d:%d:%s:%s", type, size, source, data);
    return msg_str;
}

char username[MAX_NAME];
int login(){
  char password[MAX_DATA], address[100], port[100];
  scanf("%s %s %s %s", username, password, address, port);

  int sockfd, rv, c;
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  rv = getaddrinfo(address, port, &hints, &servinfo);
  if (rv != 0) return -1;

  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  if (sockfd == -1) return -2;
  c = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
  if (servinfo == NULL || c == -1){ close(sockfd); return -3;}
  freeaddrinfo(servinfo);

  char* data = format_data(0, strlen(password), username, password);
  send(sockfd, data, MAX_PACKETSIZE, 0);
  free(data);

  char response_string[MAX_PACKETSIZE];
  int bytes = recv(sockfd, response_string, MAX_PACKETSIZE, 0);
  response_string[bytes] = '\0';
  if (!handlePacket(response_string)){ close(sockfd); sockfd = -1;}

  return sockfd;
}

int main(){
    bool quit = false;
    char command[128];
    int sockfd = -1, max_fd = -1;
    fd_set master, read_fds;
    FD_ZERO(&master);
    FD_SET(0, &master);

    logLoop: while (!quit){
      scanf("%s", command);

      if (!strcmp(command, "/login") || !strcmp(command, "lgn")){
        sockfd = login();
        if (sockfd == -1) continue;
        FD_SET(sockfd, &master);
        max_fd = sockfd + 1;
        break;
      } else if (!strcmp(command, "/quit")){
        return 0;
      }
    }

    while (!quit){
      read_fds = master;
  		int status = select(max_fd, &read_fds, NULL, NULL, NULL);

  		if (FD_ISSET(sockfd, &read_fds)){
  			char response_string[MAX_PACKETSIZE];
  			int bytes = recv(sockfd, response_string, MAX_PACKETSIZE, 0);
        response_string[bytes] = '\0';

        handlePacket(response_string);
        unsigned type = atoi(response_string);
        if (type == 13) break;
        fflush(stdout);

  		} else if (FD_ISSET(0, &read_fds)){
        fgets (command, 128, stdin);
        char* rem = command;
        while (*rem != '\n') rem++;
        *rem = 0;
        if (strlen(command) == 0) continue;

        if (strstr(command, "/quit") != NULL) {quit = true; return 0;}
        if (strstr(command, "/logout") != NULL) {break;}
        if (strstr(command, "/joinsession") != NULL) {
          char* omitPrefix = command;
          while (*omitPrefix != ' ') omitPrefix++;
          omitPrefix++;
          char* packet = format_data(4,
            strlen(omitPrefix), username, omitPrefix);
          send(sockfd, packet, strlen(packet), 0);

          char response_string[MAX_PACKETSIZE];
          int bytes = recv(sockfd, response_string, MAX_PACKETSIZE, 0);
          response_string[bytes] = '\0';
          handlePacket(response_string);
          continue;
        }
        if (strstr(command, "/leavesession") != NULL) {
          char* packet = format_data(7, 1, username, " ");
          send(sockfd, packet, strlen(packet), 0);
          continue;
        }
        if (strstr(command, "/list") != NULL) {
          char* packet = format_data(3, 1, username, " ");
          send(sockfd, packet, strlen(packet), 0);
          continue;
        }
        if (strstr(command, "/createsession") != NULL) {
          char* omitPrefix = command;
          while (*omitPrefix != ' ') omitPrefix++;
          omitPrefix++;
          char* packet = format_data(8,
            strlen(omitPrefix), username, omitPrefix);
          send(sockfd, packet, strlen(packet), 0);

          char response_string[MAX_PACKETSIZE];
          int bytes = recv(sockfd, response_string, MAX_PACKETSIZE, 0);
          response_string[bytes] = '\0';
          handlePacket(response_string);
          continue;
        }
        char* packet = format_data(10, strlen(command), username, command);
  			int send_info = send(sockfd, packet, strlen(packet), 0);
      }
    }
    close(sockfd);
    goto logLoop;
}
