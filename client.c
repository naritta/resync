
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
