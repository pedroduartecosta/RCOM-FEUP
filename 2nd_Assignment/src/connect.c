#include "connection.h"

#define MAXDATASIZE 100 // max number of bytes we can get at once

int connect_to_server(int socket, struct addrinfo *res) {
  if (connect(socket, res->ai_addr, res->ai_addrlen) < 0) {
    perror("connect()");
    exit(0);
  }
  return 1;
}

int login_to_server(int sockfd, url *url_info) {

  char *answer = malloc(100 * sizeof(char));
  char *answer2 = malloc(100 * sizeof(char));
  char * user_password_command = malloc(10 * sizeof(char) + strlen(url_info->password));
  char * user_login_command = malloc(10 * sizeof(char) + strlen(url_info->user));

  strcat(user_login_command, "USER ");
  strcat(user_login_command, url_info->user);
  strcat(user_login_command, "\r\n");
  printf("%s\n", user_login_command);


  write_to_server(sockfd, user_login_command);

  read_from_server(sockfd, answer);

  printf("%s\n",answer );

  free(answer);

  strcat(user_password_command, "PASS ");
  strcat(user_password_command, url_info->password);
  strcat(user_password_command, "\r\n");
  printf("PASS: %s\n",user_password_command );


    write_to_server(sockfd, user_password_command);

    read_from_server(sockfd, answer);

    printf("%s\n",answer);


  return 1;
}

int set_PASV_mode(int sockfd, char *answer){
  char pasv_command[9]="PASV \r\n";

  write_to_server(sockfd, pasv_command);

  read_from_server(sockfd, answer);

  return 1;
}

int get_ip_adress(char *answer3, char ip[100], char port1[100], char port2[100]){

  char *ret;
  ret=strchr(answer3, '(');

  ret++;

  get_string(ret, 4, ip);

  ret += strlen(ip);

  get_string(ret, 2, port1);

  ret += strlen(port1);

  get_string(ret, 2, port2);

  memmove(port1, port1+1, strlen(port1));
  memmove(port2, port2+1, strlen(port2));

  return 1;

}

int get_string(char *info, int numberOfCommas, char ret[100]){

  int i=0;
  int atualNumberOfCommas=0;
  while(1){
    if(*info == ',' || *info == ')'){
      atualNumberOfCommas++;
      if (atualNumberOfCommas == numberOfCommas){
        break;
      }
      else ret[i] = '.';
    }
    else {
      ret[i] = *info;
    }

    i++;
    info++;
  }

  ret[i] = '\0';


  return 1;
}

int write_to_server(int sockfd, const char *message) {
  int bytes = 0;

  bytes = write(sockfd, message, strlen(message));

  if (bytes == strlen(message))
    return 1;
  perror("Writting to server.");
}

int read_from_server(int sockfd, char * answer) {

  int numbytes = 0;

  if ((numbytes = recv(sockfd, answer, MAXDATASIZE - 1, 0)) == -1) {
    perror("Receiving from server");
    exit(1);
  }

  answer[numbytes] = '\0';

  return 1;
}
