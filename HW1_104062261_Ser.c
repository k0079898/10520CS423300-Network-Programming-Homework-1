#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_SIZE 2048
#define MAX_CONNECTION 100

void connection_handler(int sockfd);
void hello_msg_handler(int sockfd);
void command_handler(int sockfd, char cmd[]);
void file_listing_handler(int sockfd);
void file_sending_handler(int sockfd, char filename[]);
void file_getting_handler(int sockfd, char filename[]);


int main(int argc, char **argv) {
  int svr_fd;
  struct sockaddr_in svr_addr;

  int cli_fd;
  struct sockaddr_in cli_addr;
  socklen_t addr_len;

  if(argc < 2) {
    printf("Usage: ./server <Port>\n");
    exit(1);
  }

  svr_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (svr_fd < 0) {
    perror("Create socket failed.");
    exit(1);
  }

  bzero(&svr_addr, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  svr_addr.sin_port = htons(atoi(argv[1]));

  if (bind(svr_fd, (struct sockaddr*)&svr_addr , sizeof(svr_addr)) < 0) {
    perror("Bind socket failed.");
    exit(1);
  }

  if (listen(svr_fd, MAX_CONNECTION) < 0) {
    perror("Listen socket failed.");
    exit(1);
  }

  printf("Server started\n");
  printf("Maximum connections set to %d\n", MAX_CONNECTION);

  mkdir("./Upload", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  printf("Upload directory was created\n");

  printf("Listening on %s:%d\n", inet_ntoa(svr_addr.sin_addr), atoi(argv[1]));
  printf("Waiting for client......\n\n");

  while(1) {
    addr_len = sizeof(cli_addr);
    cli_fd = accept(svr_fd, (struct sockaddr*)&cli_addr, (socklen_t*)&addr_len);

    if (cli_fd < 0) {
      perror("Accept failed");
      exit(1);
    }

    printf("[INFO] Client is from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    if(fork() == 0){
      close(svr_fd);
      connection_handler(cli_fd);
      printf("[INFO] Client %s:%d terminated\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
      exit(0);
    }

    close(cli_fd);
  }

  close(svr_fd);
  return 0;
}

void connection_handler(int sockfd) {
  char cmd[10], info[MAX_SIZE], buf[MAX_SIZE], cwd[MAX_SIZE];
  memset(buf, '\0', MAX_SIZE);

  hello_msg_handler(sockfd);

  getcwd(cwd, MAX_SIZE);
  sprintf(buf, "%s$ ", cwd);
  write(sockfd, buf, MAX_SIZE);
  memset(buf, '\0', MAX_SIZE);

  while ((read(sockfd, buf, MAX_SIZE)) > 0) {
    sscanf(buf, "%s", cmd);
    printf("[INFO] Client send %s request\n", cmd);
    if (strcmp(cmd, "exit") == 0) {
      break;
    }else if (strcmp(cmd, "cd") == 0) {
      sscanf(buf, "%*s%s", info);
      if (chdir(info) < 0) sprintf(buf, "[x] Failed to change directory.\n");
      else sprintf(buf, "[✓] Directory changed to '%s'.\n", info);
      write(sockfd, buf, MAX_SIZE);
    }else if (strcmp(cmd, "ls") == 0) {
      file_listing_handler(sockfd);
    }else if (strcmp(cmd, "upload") == 0) {
      sscanf(buf, "%*s%s", info);
      file_getting_handler(sockfd, info);
    }else if (strcmp(cmd, "download") == 0) {
      sscanf(buf, "%*s%s", info);
      file_sending_handler(sockfd, info);
    }else {
      sprintf(buf, "[x] Incorrect command.\n");
      write(sockfd, buf, MAX_SIZE);
    }
    memset(cmd, '\0', 10);
    memset(buf, '\0', MAX_SIZE);
    getcwd(cwd, MAX_SIZE);
    sprintf(buf, "%s$ ", cwd);
    write(sockfd, buf, MAX_SIZE);
    memset(buf, '\0', MAX_SIZE);
  }
}

void hello_msg_handler(int sockfd) {
  char buf[MAX_SIZE];

  printf("[INFO] Send hello msg to client\n");

  sprintf(buf, "%s", "[✓] Connect to server.\n[✓] Server reply!\n--------------------\n  Control command:\n  1. cd <Directory>\n  2. ls\n  3. upload <FileName>\n  4. download <FileName>\n  5. exit\n--------------------\n");
  if (write(sockfd, buf, MAX_SIZE) < 0) {
      perror("Write failed!\n");
  }
}

void file_listing_handler(int sockfd) {
  DIR* pDir;
  struct dirent* pDirent = NULL;
  char buf[MAX_SIZE], cwd[MAX_SIZE];

  printf("[INFO] List file to client\n");

  getcwd(cwd, MAX_SIZE);
  if ((pDir = opendir(cwd)) == NULL) {
      perror("Open directory failed\n");
  }

  memset(buf, '\0', MAX_SIZE);
  sprintf(buf, "%s", "------Directory file------\n");
  write(sockfd, buf, strlen(buf));
  while ((pDirent = readdir(pDir)) != NULL) {
      if (strcmp(pDirent->d_name, ".") == 0 || strcmp(pDirent->d_name, "..") == 0) {
        continue;
      }

      sprintf(buf, "%s", pDirent->d_name);
      write(sockfd, buf, strlen(buf));
      sprintf(buf, "%s", "\n");
      write(sockfd, buf, strlen(buf));
  }
  sprintf(buf, "%s", "--------------------------\n");
  write(sockfd, buf, strlen(buf));
  closedir(pDir);
}

void file_sending_handler(int sockfd, char filename[]) {
  char fail_msg[21] = "[x] Download failed.\n";
  char buf[MAX_SIZE];
  char path[MAX_SIZE];
  char cwd[MAX_SIZE];
  FILE *fp;

  int file_size = 0;
  unsigned int write_byte = 0;
  int write_sum = 0;

  getcwd(cwd, MAX_SIZE);
  sprintf(path, "%s/%s", cwd, filename);
  fp = fopen(path, "rb");
  if (fp) {
    memset(buf, '\0', MAX_SIZE);

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    if (file_size == -1) {
      sprintf(buf, "[x] Cannot download a directory.\n");
      write(sockfd, buf, MAX_SIZE);
    }else {

      sprintf(buf, "[-] Downloading `%s`. Size:%d ......\n", filename, file_size);
      write(sockfd, buf, MAX_SIZE);

      memset(buf, '\0', MAX_SIZE);
      sprintf(buf, "%d", file_size);
      write(sockfd, buf, MAX_SIZE);

      write_sum = 0;
      while (write_sum < file_size) {

        memset(buf, '\0', MAX_SIZE);
        write_byte = fread(&buf, sizeof(char), MAX_SIZE, fp);

        write(sockfd, buf, write_byte);
        write_sum += write_byte;
      }

      fclose(fp);

      sleep(2);

      memset(buf, '\0', MAX_SIZE);
      printf("[INFO] Sent successfully\n");
      sprintf(buf, "%s", "[✓] Download complete!\n");
      write(sockfd, buf, strlen(buf));
    }
  } else {
    printf("[ERROR] Download failed\n");
    write(sockfd, fail_msg, MAX_SIZE);
  }
  return;
}

void file_getting_handler(int sockfd, char filename[]) {
  char buf[MAX_SIZE];
  char path[MAX_SIZE];
  char cwd[MAX_SIZE];
  char info[10];

  int file_size = 0;
  int read_byte = 0;
  int read_sum = 0;

  FILE *fp;

  memset(buf, '\0', MAX_SIZE);
  read(sockfd, buf, MAX_SIZE);
  printf("%s", buf);

  sscanf(buf, "%s", info);
  if (strcmp(info, "[ERROR]")!=0) {

    memset(buf, '\0', MAX_SIZE);
    read(sockfd, buf, MAX_SIZE);
    file_size = atoi(buf);

    memset(path, '\0', MAX_SIZE);

    getcwd(cwd, MAX_SIZE);
    sprintf(path, "%s/%s", cwd, filename);

    read_sum = 0;
    fp = fopen(path, "wb");
    if (fp) {
        while (read_sum < file_size) {
          memset(buf, '\0', MAX_SIZE);

          read_byte=read(sockfd, buf, MAX_SIZE);
          buf[read_byte]='\0';

          fwrite(&buf, sizeof(char), read_byte, fp);
          read_sum += read_byte;
        }
        fclose(fp);

        memset(buf, '\0', MAX_SIZE);
        read(sockfd, buf, MAX_SIZE);
        printf("%s", buf);

    } else {
      printf("[ERROR] Allocate memory fail\n");
    }
  }
  return;
}
