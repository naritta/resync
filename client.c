
int connect_server() {
  int ret_code = 0;
  char buf[AVG_SIZE];
  uint32_t buf_len = 0;
  int fd = -1;
  ssize_t size = 0;
  int response = -1;

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    printf("failed to create_socket(errno=%d:%s)\n", errno, strerror(errno));
    return -1;
  }

  sin.sin_family = AF_INET;
	sin.sin_port = htons(SERVER_PORT);
  sin.sin_addr.s_addr = inet_addr(SERVER_ADDR);

  ret_code = connect(fd, (const struct sockaddr *)&sin, sizeof(sin));
  if (ret_code == -1) {
    printf("failed to create_socket(errno=%d:%s)\n", errno, strerror(errno));
    close(fd);
    return -1;
  }

	bool authRes = authenticate("apikey1234", fd);
	if (authRes) {
		printf("authentication succeeded.\n");
	} else {
		printf("authentication failed.\n");
		close(fd);
		return -1;
	}

	char data[AVG_SIZE];
	while (1){
		printf("waiting sync request...\n" );

		char r_str[AVG_SIZE];
	  size = recv(fd, &r_str, sizeof(r_str), 0);
		char *filename;
		char *filesize;

		// filename is in right before first comma
	  filename = strtok(r_str, ",");
		// filesize is in right before second comma
		filesize = strtok(NULL, ",");

		char pathname[MAX_PATH] = "./";
		strcat(pathname, filename);

		bool alreadyhas = false;

		char data[AVG_SIZE] = "";
		// if filesize is -1, it's directy
		if (strcmp(filesize, "-1")==0) {
			char s_str[16] = "directry";
			send(fd, &s_str, strlen(s_str), 0);

		  size = recv(fd, &data, sizeof(data), 0);

			printf("Trying to ctreate directry: %s\n", pathname);
			mkdir(pathname, S_IRWXU);
		}
		else if (existFile(pathname) && atol(filesize)==getFileSize(pathname)) {
			// if same file name exists and size is same, see they are same.
			printf("Receiving and existing file size is same: %s\n", filesize);
			char s_str[ACK_SIZE] = "exists";
			alreadyhas = true;
			send(fd, &s_str, strlen(s_str), 0);

		  size = recv(fd, &data, sizeof(data), 0);

			printf("already file exists: %s\n", pathname);
		// if same file name exists and size is same, see they are same.
		} else {
			printf("receiving file size: %s\n", filesize);
			printf("existing file size: %ld\n", getFileSize(pathname));
			char s_str[ACK_SIZE] = "OK";
			alreadyhas = false;
			send(fd, &s_str, strlen(s_str), 0);

			printf("Trying to create file: %s\n", pathname);
			int fp = open(pathname, O_WRONLY | O_CREAT, 0600);
		  if (fp < 0) {
		    perror("Opening file failed");
		    return 1;
		  }

			int n;
			while ((n = read(fd, &data, sizeof(data))) > 0) {
		    int ret = write(fp, data, n);
		    if (n < sizeof(data)) {
		      break;
		    }
		  }

			printf("Done createing file: %s\n", pathname);
		}

		char one_sync_done[ACK_SIZE] = "One sync done";
		send(fd, &one_sync_done, strlen(one_sync_done), 0);
	}

  close(fd);

  return 0;
}

// find all files and directries recuresively, and send it's header and data
void send_dir(char *dir, int sockfd)
{
   char path[MAX_PATH];
   struct dirent *dp;
   DIR *dfd;
	 char s_str[AVG_SIZE];
	 char r_str[AVG_SIZE];

   if ((dfd = opendir(dir)) == NULL) {
      fprintf(stderr, "can't open %s\n", dir);
      return;
   }

	 sprintf(s_str, "%s,%s,", dir, "-1");
	 send(sockfd, &s_str, strlen(s_str), 0);
	 recv(sockfd, r_str, AVG_SIZE, 0);
	 char dummy_str[ACK_SIZE] = "dummy";
	 send(sockfd, dummy_str, strlen(dummy_str), 0);
	 char allow_sync[AVG_SIZE];
	 recv(sockfd, allow_sync, AVG_SIZE, 0);

   while ((dp = readdir(dfd)) != NULL) {
      if (dp->d_type == DT_DIR){
         path[0] = '\0';
         if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue;
         sprintf(path, "%s/%s", dir, dp->d_name);
				 printf("sending %s\n", path);
				 // if it's directry, send filesize as -1 and notify receiver that.
				 // concat filename and filesize using comma, and send it as header
				 sprintf(s_str, "%s,%s,", path, "-1");
				 send(sockfd, &s_str, strlen(s_str), 0);
				 recv(sockfd, r_str, AVG_SIZE, 0);
				 char dummy_str[ACK_SIZE] = "dummy";
				 send(sockfd, dummy_str, strlen(dummy_str), 0);
				 char ack_str[AVG_SIZE];
	 			 recv(sockfd, ack_str, 48, 0);

				 send_dir(path, sockfd);
      } else {
				sprintf(path, "%s/%s", dir, dp->d_name);
				printf("sending %s\n", path);
				sprintf(s_str, "%s,%li,", path, getFileSize(path));

				send(sockfd, &s_str, strlen(s_str), 0);
	 			recv(sockfd, r_str, AVG_SIZE, 0);
	 			FILE *fp = fopen(path, "r");
				char data[MAX_DATA_SIZE];
				int n;
				while ( (n = fread(data, sizeof(unsigned char), sizeof(data) / sizeof(data[0]), fp)) > 0 ) {
			    int ret = write(sockfd, data, n);
			    if (ret < 1) {
			      perror("write error for sending file.");
			      break;
			    }
			  }
				char one_done[AVG_SIZE];
				recv(sockfd, one_done, AVG_SIZE, 0);
			}
   }
   closedir(dfd);
}

int sync_dir() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		exit(1);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SYNC_PORT);
	addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

	connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	// send all directries and files under specified directy.
	send_dir("./", sockfd);

	char done[5] = "DONE";
	send(sockfd, done, strlen(done), 0);
	printf("%s", done);

	close(sockfd);

	return 0;
}

int main(int argc, char *argv[]){
	if (argc < 2) {
		printf("ERROR: please specify in arguments");
		return 0;
	} else if (strcmp(argv[1], "sync") == 0) {
		int ret = sync_dir();
		return ret;
	} else if (strcmp(argv[1], "connect") == 0) {
		int ret = connect_server();
	} else {
		printf("ERROR: Please use sync or connect in arguments");
		return 0;
	}
}
