#include "../inc/cmu_tcp.h"

/*
 * Param: dst - The structure where socket information will be stored
 * Param: flag - A flag indicating the type of socket(Listener / Initiator)
 * Param: port - The port to either connect to, or bind to. (Based on flag)
 * Param: ServerIP - The server IP to connect to if the socket is an initiator.
 *
 * Purpose: To construct a socket that will be used in various connections.
 *  The initiator socket can be used to connect to a listener socket.
 *
 * Return: The newly created socket will be stored in the dst parameter,
 *  and the value returned will provide error information. 
 *
 */
void Shakehands(cmu_socket_t *dst) {
	switch (dst->type)
	{
	case TCP_INITATOR: {
		int len = 0;
		int seq = 0;
		socklen_t conn_len = sizeof(dst->conn);
		char* SYNpk = create_packet_buf(dst->my_port, dst->their_port, seq, 1,
			DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK, 1, 0, NULL, NULL, 0);
		len = DEFAULT_HEADER_LEN;

		sendto(dst->socket, SYNpk, len, 0, (struct sockaddr*) &(dst->conn), sizeof(dst->conn));//第一次握手
		dst->state = SYN_SENT;
		printf("SYN send\n");

		char hdr[DEFAULT_HEADER_LEN];
		int revn;
		revn = recvfrom(dst->socket, hdr, DEFAULT_HEADER_LEN, NO_FLAG,
			(struct sockaddr*) &(dst->conn), &conn_len);//第二次握手  


		uint32_t ack = get_ack(hdr);
		uint8_t flags = get_flags(hdr);
		uint32_t seq1 = get_seq(hdr);
		//printf("ACK get %d\n", ack);
		//printf("flag1%d\n", flags == (ACK_FLAG_MASK&SYN_FLAG_MASK));

		if (flags == (ACK_FLAG_MASK&SYN_FLAG_MASK) && ack == seq + 1) {
			char* ACKpk = create_packet_buf(dst->my_port, dst->their_port, seq1, seq1 + 1,
				DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, 1, 0, NULL, NULL, 0);
			sendto(dst->socket, ACKpk, len, 0, (struct sockaddr*) &(dst->conn), sizeof(dst->conn));//第三次握手
			int ret = connect(dst->socket, (struct sockaddr*) &(dst->conn), sizeof(dst->conn));
			printf("ACK ack\n-----------------------------------\n");
		}



		dst->state = CONN; }
					   break;

	case TCP_LISTENER:
	{
		char hdr0[DEFAULT_HEADER_LEN];
		int revn0;
		socklen_t conn_len = sizeof(dst->conn);
		revn0 = recvfrom(dst->socket, hdr0, DEFAULT_HEADER_LEN, NO_FLAG,
			(struct sockaddr*) &(dst->conn), &conn_len);//第二次握手  
		printf("%d\n",revn0);
		uint8_t flags0 = get_flags(hdr0);
		uint32_t Seq0 = get_seq(hdr0);
		dst->state = SYN_RCVD;
		printf("SYN recv\n");

		if (flags0 == SYN_FLAG_MASK) {
			char* ACKpk = create_packet_buf(dst->my_port, dst->their_port, 2, Seq0 + 1,
				DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK&SYN_FLAG_MASK, 1, 0, NULL, NULL, 0);
			sendto(dst->socket, ACKpk, DEFAULT_HEADER_LEN, 0, (struct sockaddr*) &(dst->conn), sizeof(dst->conn));//第二次握手
			printf("SYN ACK send %d\n", Seq0 + 1);
		}

		int revn1;
		char hdr1[DEFAULT_HEADER_LEN];
		revn1 = recvfrom(dst->socket, hdr1, DEFAULT_HEADER_LEN, NO_FLAG,
			(struct sockaddr*) &(dst->conn), &conn_len);//第三次握手 
		uint8_t flags1 = get_flags(hdr1);
		uint32_t Seq1 = get_seq(hdr1);
		uint32_t ack1 = get_ack(hdr1);
		//printf("ack_received:%d\n", ack1);
		if (flags1 == ACK_FLAG_MASK && ack1 == 3) {
			printf("Ready to connect\n-----------------------------------\n");//连接
			dst->state = CONN;
		}
	}
	break;

	default:
		perror("Unknown Flag");
		return EXIT_ERROR;
	}
}



int cmu_socket(cmu_socket_t * dst, int flag, int port, char * serverIP){
  int sockfd, optval;
  socklen_t len;
  struct sockaddr_in conn, my_addr;
  len = sizeof(my_addr);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0){
    perror("ERROR opening socket");
    return EXIT_ERROR;
  }
  dst->their_port = port;
  dst->socket = sockfd;
  dst->received_buf = NULL;
  dst->received_len = 0;
  pthread_mutex_init(&(dst->recv_lock), NULL);
  dst->sending_buf = NULL;
  dst->sending_len = 0;
  pthread_mutex_init(&(dst->send_lock), NULL);
  dst->type = flag;
  dst->dying = FALSE;
  pthread_mutex_init(&(dst->death_lock), NULL);
  dst->window.rwnd = MAX_NETWORK_BUFFER-1;
  dst->window.last_ack_received = 0;
  dst->window.last_seq_received = 0;
  dst->window.last_len_received = 0;
  dst->window.rwnd = 999999999999;
  dst->window.window_size = WINDOW_INITIAL_WINDOW_SIZE*1460;
  dst->window.window_ssthresh = WINDOW_INITIAL_SSTHRESH*1024;
  dst->window.dupACKcount = 0;
  dst->window.RTT = WINDOW_INITIAL_RTT;
  dst->window.MSS = 1460;
  dst->window.DevRTT = 0;
  dst->window.RTO = 3000000;
  dst->window.wd_state = slow_start;
  dst->window.un_acked_len = 0;
  dst->window.recv_buffer_head = NULL;
  dst->window.recv_buffer_tail = NULL;
  dst->window.send_buffer_head = NULL;
  dst->window.send_buffer_tail = NULL;
  //dst->window.send_buffer_head = malloc(sizeof(send_package_buffer_t));
  pthread_mutex_init(&(dst->window.ack_lock), NULL);

  if(pthread_cond_init(&dst->wait_cond, NULL) != 0){
    perror("ERROR condition variable not set\n");
    return EXIT_ERROR;
  }


  switch(flag){
    case(TCP_INITATOR):
      if(serverIP == NULL){
        perror("ERROR serverIP NULL");
        return EXIT_ERROR;
      }
      memset(&conn, 0, sizeof(conn));          
      conn.sin_family = AF_INET;          
      conn.sin_addr.s_addr = inet_addr(serverIP);  
      conn.sin_port = htons(port); 
      dst->conn = conn;

      my_addr.sin_family = AF_INET;
      my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      my_addr.sin_port = 0;
      if (bind(sockfd, (struct sockaddr *) &my_addr, 
        sizeof(my_addr)) < 0){
        perror("ERROR on binding");
        return EXIT_ERROR;
      }

      break;
    
    case(TCP_LISTENER):
      bzero((char *) &conn, sizeof(conn));
      conn.sin_family = AF_INET;
      conn.sin_addr.s_addr = htonl(INADDR_ANY);
      conn.sin_port = htons((unsigned short)port);

      optval = 1;
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
           (const void *)&optval , sizeof(int));
      if (bind(sockfd, (struct sockaddr *) &conn, 
        sizeof(conn)) < 0){
          perror("ERROR on binding");
          return EXIT_ERROR;
      }
      dst->conn = conn;
      break;

    default:
      perror("Unknown Flag");
      return EXIT_ERROR;
  }
  Shakehands(dst);
  getsockname(sockfd, (struct sockaddr *) &my_addr, &len);
  dst->my_port = ntohs(my_addr.sin_port);

  pthread_create(&(dst->thread_id), NULL, begin_backend, (void *)dst);  
  return EXIT_SUCCESS;
}

/*
 * Param: sock - The socket to close.
 *
 * Purpose: To remove any state tracking on the socket.
 *
 * Return: Returns error code information on the close operation.
 *
 */
int cmu_close(cmu_socket_t * sock){
  while(pthread_mutex_lock(&(sock->death_lock)) != 0);
  sock->dying = TRUE;
  pthread_mutex_unlock(&(sock->death_lock));

  pthread_join(sock->thread_id, NULL); 

  if(sock != NULL){
    if(sock->received_buf != NULL)
      free(sock->received_buf);
    if(sock->sending_buf != NULL)
      free(sock->sending_buf);
  }
  else{
    perror("ERORR Null scoket\n");
    return EXIT_ERROR;
  }
  return close(sock->socket);
}

/*
 * Param: sock - The socket to read data from the received buffer.
 * Param: dst - The buffer to place read data into.
 * Param: length - The length of data the buffer is willing to accept.
 * Param: flags - Flags to signify if the read operation should wait for
 *  available data or not.
 *
 * Purpose: To retrive data from the socket buffer for the user application.
 *
 * Return: If there is data available in the socket buffer, it is placed
 *  in the dst buffer, and error information is returned. 
 *
 */
int cmu_read(cmu_socket_t * sock, char* dst, int length, int flags){
  char* new_buf;
  int read_len = 0;

  if(length < 0){
    perror("ERROR negative length");
    return EXIT_ERROR;
  }

  while(pthread_mutex_lock(&(sock->recv_lock)) != 0);

  switch(flags){
    case NO_FLAG:
      while(sock->received_len == 0){
        pthread_cond_wait(&(sock->wait_cond), &(sock->recv_lock)); 
      }
    case NO_WAIT:
      if(sock->received_len > 0){
        if(sock->received_len > length)
          read_len = length;
        else
          read_len = sock->received_len;

		printf("ckpt1\n");
        memcpy(dst, sock->received_buf, read_len);
        if(read_len < sock->received_len){
           new_buf = malloc(sock->received_len - read_len);
           memcpy(new_buf, sock->received_buf + read_len, 
            sock->received_len - read_len);
           free(sock->received_buf);
           sock->received_len -= read_len;
           sock->received_buf = new_buf;
        }
        else{
          free(sock->received_buf);
          sock->received_buf = NULL;
          sock->received_len = 0;
        }
		printf("ckpt2\n");
      }
      break;
    default:
      perror("ERROR Unknown flag.\n");
      read_len = EXIT_ERROR;
  }
  pthread_mutex_unlock(&(sock->recv_lock));
  return read_len;
}

/*
 * Param: sock - The socket which will facilitate data transfer.
 * Param: src - The data source where data will be taken from for sending.
 * Param: length - The length of the data to be sent.
 *
 * Purpose: To send data to the other side of the connection.
 *
 * Return: Writes the data from src into the sockets buffer and
 *  error information is returned. 
 *
 */
int cmu_write(cmu_socket_t * sock, char* src, int length){
  while(pthread_mutex_lock(&(sock->send_lock)) != 0);
  if(sock->sending_buf == NULL)
    sock->sending_buf = malloc(length);
  else
    sock->sending_buf = realloc(sock->sending_buf, length + sock->sending_len);
  memcpy(sock->sending_buf + sock->sending_len, src, length);
  sock->sending_len += length;
  //printf("sending_len:%d\n", sock->sending_len);

  pthread_mutex_unlock(&(sock->send_lock));
  return EXIT_SUCCESS;
}

