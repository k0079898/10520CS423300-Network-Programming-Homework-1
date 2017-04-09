#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_SIZE 2048

void connection_handler (int sockfd);
void file_upload_handler(int sockfd, char filename[]);
void file_download_handler(int sockfd, char filename[]);

int main (int argc, char **argv) {
  int cli_fd;
  struct sockaddr_in svr_addr;

  cli_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_fd < 0) {
    perror("Create socket failed.");
    exit(1);
  }

  bzero(&svr_addr, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_port = htons(atoi(argv[2]));
  if (inet_pton(AF_INET, argv[1], &svr_addr.sin_addr) <= 0) {
     perror("Address converting fail with wrong address argument");
     return 0;
  }

  if (connect(cli_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0) {
    perror("Connect failed");
    exit(1);
  }

  connection_handler(cli_fd);

  close(cli_fd);
  return 0;
}

void connection_handler (int sockfd) {
    char input[MAX_SIZE];
    char path[MAX_SIZE];
    char buf[MAX_SIZE];
    char filename[MAX_SIZE];
    char cmd[10];

    int read_bytes;

    sprintf(path, "./Download");
    mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
    printf("[✓] Download directory was created.\n");

    memset(buf, '\0', MAX_SIZE);
    read(sockfd, buf, MAX_SIZE);
    printf("%s", buf);

    while (1) {
        read_bytes=read(sockfd, buf, sizeof(buf));
        buf[read_bytes] = '\0';
        printf("%s", buf);
        fgets(input, MAX_SIZE, stdin);
        sscanf(input, "%s", cmd);
        if (strcmp(cmd, "exit") == 0) {
            break;
        }else if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "ls") == 0) {
          write(sockfd, input, strlen(input));
          read_bytes=read(sockfd, buf, sizeof(buf));
          buf[read_bytes] = '\0';
          printf("%s", buf);
        }else if (strcmp(cmd, "upload") == 0) {
          sscanf(input, "%*s%s", filename);
          write(sockfd, input, strlen(input));
          file_upload_handler(sockfd, filename);
        }else if (strcmp(cmd, "download") == 0) {
          sscanf(input, "%*s%s", filename);
          write(sockfd, input, strlen(input));
          file_download_handler(sockfd, filename);
        }else {
          write(sockfd, input, strlen(input));
          read_bytes=read(sockfd, buf, sizeof(buf));
          buf[read_bytes] = '\0';
          printf("%s", buf);
        }
    }
    printf("[x] Socket closed.\n");
}

void file_upload_handler(int sockfd, char filename[]) {
  char fail_msg[23] = "[ERROR] Upload failed\n";
  char buf[MAX_SIZE];
  char path[MAX_SIZE];
  FILE *fp;

  int file_size = 0;
  unsigned int write_byte = 0;
  int write_sum = 0;

  sprintf(path, "./Download/%s", filename);
  fp = fopen(path, "rb");
  if (fp) {
    memset(buf, '\0', MAX_SIZE);

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    if (file_size == -1) {
      printf(buf, "[x] Cannot upload a directory.\n");
      sprintf(buf, "[ERROR] Upload terminated\n");
      write(sockfd, buf, MAX_SIZE);
    }else {
      printf("[-] Uploading `%s`. Size:%d ......\n", filename, file_size);
      sprintf(buf, "[INFO] Receiving file\n");
      write(sockfd, buf, MAX_SIZE);

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
      printf("[✓] Upload complete!\n");
      sprintf(buf, "[INFO] File has been received successfully\n");
      write(sockfd, buf, MAX_SIZE);
    }
  }else {
    printf("[x] Upload failed\n");
    write(sockfd, fail_msg, MAX_SIZE);
  }
}

void file_download_handler(int sockfd, char filename[]) {
  char buf[MAX_SIZE];
  char path[MAX_SIZE];
  char info[10];

  int file_size = 0;
  int read_byte = 0;
  int read_sum = 0;
  FILE *fp;

  memset(buf, '\0', MAX_SIZE);
  read(sockfd, buf, MAX_SIZE);
  printf("%s", buf);

  sscanf(buf, "%s", info);
  if (strcmp(info, "[x]")!=0) {

    memset(buf, '\0', MAX_SIZE);
    read(sockfd, buf, MAX_SIZE);
    file_size = atoi(buf);

    memset(path, '\0', MAX_SIZE);
    sprintf(path, "./Download/%s", filename);

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
      perror("Allocate memory fail\n");
    }
  }
}
