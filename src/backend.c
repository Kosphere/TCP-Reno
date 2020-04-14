#include "../inc/backend.h"

/*
* Param: sock - The socket to check for acknowledgements.
* Param: seq - Sequence number to check
*
* Purpose: To tell if a packet (sequence number) has been acknowledged.
*
*/
int check_ack(cmu_socket_t * sock, uint32_t seq) {
	int result;
	while (pthread_mutex_lock(&(sock->window.ack_lock)) != 0);
	if (sock->window.last_ack_received > seq)
		result = TRUE;
	else
		result = FALSE;
	pthread_mutex_unlock(&(sock->window.ack_lock));
	return result;
}

/*
* Param: sock - The socket used for handling packets received
* Param: pkt - The packet data received by the socket
*
* Purpose: Updates the socket information to represent
*  the newly received packet.
*
* Comment: This will need to be updated for checkpoints 1,2,3
*
*/
void handle_message(cmu_socket_t * sock, char* pkt) {
	//printf("handle message!\n");
	char* rsp;
	uint8_t flags = get_flags(pkt);
	uint32_t seq;
	socklen_t conn_len = sizeof(sock->conn);
	switch (flags) {
	case ACK_FLAG_MASK:
		//printf("recv_ack:%d\n", get_ack(pkt));
		if (get_ack(pkt)>= sock->window.last_ack_received)
			sock->window.rwnd = get_advertised_window(pkt);
			//printf("rwnd大小：%d\n", sock->window.rwnd);
			//printf("window_size:%d\n send_len:%d\n", sock->window.window_size,sock->window.un_acked_len);
		send_package_buffer_t* head = sock->window.send_buffer_head;
		if (head != NULL) {
			//printf("发送缓存head_seq：%d\n", head->seq);
		}
		
		//printf("s1\n");
		if (get_ack(pkt) > sock->window.last_ack_received) {
			//printf("stage-1");
			//new ack
			if (sock->window.wd_state == slow_start) {
				sock->window.window_size = (sock->window.window_size) + (sock->window.MSS);//未达到阈值累加MSS
				sock->window.dupACKcount = 0;//重置ack重复次数
			}
			else if (sock->window.wd_state == congestion_avoi) {
				sock->window.window_size += (uint32_t)(sock->window.MSS)*((sock->window.MSS) / (sock->window.window_size));//达到阈值累加速度变慢
				sock->window.dupACKcount = 0;//重置ack重复次数
			}
			else {
				sock->window.window_size = sock->window.window_ssthresh;//从fast recovry返回congestion avoidance
				sock->window.wd_state = congestion_avoi;
				sock->window.dupACKcount = 0;//重置ack重复次数
			}
			//printf("stage-2");
			//检测是否超过阈值
			if ((sock->window.window_size) >= (sock->window.window_ssthresh)) {
				sock->window.wd_state = congestion_avoi;//从slow start转移到congestion avoidance
			}
			//printf("stage-3");
			sock->window.last_ack_received = get_ack(pkt);
			send_package_buffer_t* head = sock->window.send_buffer_head;
			////printf("stage1\n");
			//printf("stage-4");
			send_package_buffer_t* next_node = head;
			send_package_buffer_t* next_next;
			while (next_node != NULL && (next_node->seq) < get_ack(pkt)) {
				////printf("stage2\n");
				sock->window.send_buffer_head = next_node->next;
				sock->window.un_acked_len -= get_plen(next_node->package) - get_hlen(next_node->package);
				next_next = next_node->next;
				next_node->next = NULL;
				free(next_node->package);
				next_node->package = NULL;
				free(next_node);
				next_node = next_next;
				////printf("stage3\n");
				////printf("%d\n", next_node);
			}
		}
		else if (get_ack(pkt) == sock->window.last_ack_received){
			//printf("s2\n");
			//TODO:: 快速重传
			if (sock->window.dupACKcount == 3) {
				//printf("快速重传\n");
				if (sock->window.wd_state == slow_start) {
					//printf("stage12\n");
					sock->window.wd_state = fast_recovery;
					sock->window.window_ssthresh = (sock->window.window_size) / 2;
					sock->window.window_size = (sock->window.window_size) + (sock->window.MSS) * 3;
					//重传丢失段
					send_package_buffer_t* head = sock->window.send_buffer_head;
					
					//send_package_buffer_t* next_node = head;
					//while (next_node != NULL && (get_seq(next_node->package) <= get_ack(pkt))) {
					//	next_node = next_node->next;
					//}//寻找需要重传的数据段
					if (head != NULL) {
						socklen_t conn_len = sizeof(sock->conn);//重传丢失段
						sendto(sock->socket, head->package, get_plen(head->package), 0, (struct sockaddr*) &(sock->conn), conn_len);
						//printf("stage12 %d\n", head->seq);
					}
					sock->window.dupACKcount = 0;//ack重复次数重置为0
				}
				else if (sock->window.wd_state == congestion_avoi) {
					//printf("stage13\n");
					sock->window.wd_state = fast_recovery;
					sock->window.window_ssthresh = (sock->window.window_size) / 2;
					sock->window.window_size = (sock->window.window_size) + (sock->window.MSS) * 3;
					//重传丢失段
					send_package_buffer_t* head = sock->window.send_buffer_head;
					////printf("stage13\n");
					//send_package_buffer_t* next_node = head;
					//while (next_node != NULL && (get_seq(next_node->package) < get_ack(pkt))) {
					//	next_node = next_node->next;
					//}//寻找需要重传的数据段
					if (head != NULL) {
						socklen_t conn_len = sizeof(sock->conn);//重传丢失段
						sendto(sock->socket, head->package, get_plen(head->package), 0, (struct sockaddr*) &(sock->conn), conn_len);
						//printf("stage13 %d\n", head->seq);
					}
					sock->window.dupACKcount = 0;//ack重复次数重置为0
				}
				//状态机在fast recovery条件下无操作
			}
			else {//ack重复但是不足3
				//printf("s3\n");
				if (sock->window.wd_state == slow_start) {
					sock->window.dupACKcount += 1;
				}
				else if (sock->window.wd_state == congestion_avoi) {
					sock->window.dupACKcount += 1;
				}
				else {
					sock->window.window_size = (sock->window.window_size) + (sock->window.MSS);
				}
			}


		}
		//printf("over\n");
		break;
	default:
		//sleep(1);
		//printf("recv_seq:%d\n", get_seq(pkt));
		seq = get_seq(pkt);
		if (seq >= sock->window.last_seq_received +sock->window.last_len_received || (seq == 0 && sock->window.last_seq_received == 0)) {
			recv_package_buffer_t* buffer = (recv_package_buffer_t*)calloc(1,sizeof(recv_package_buffer_t*));



			buffer->seq = seq;
			buffer->data_length = get_plen(pkt) - get_hlen(pkt);
			////printf("2343\n");
			buffer->data = calloc(buffer->data_length,sizeof(char));
			////printf("233\n");
			memcpy(buffer->data, pkt + get_hlen(pkt), buffer->data_length);
			////printf("recv_data:%s\n", buffer->data);
			////printf("recv_data_len:%d\n", buffer->data_length);
			//generate a buffer
			

			recv_package_buffer_t* head = sock->window.recv_buffer_head;
			////printf("signal:%d\n", seq == sock->window.last_seq_received + sock->window.last_len_received);
			////printf("last_seq:%d\n", sock->window.last_seq_received);
			////printf("last_len:%d\n", sock->window.last_len_received);
			if (seq == sock->window.last_seq_received + sock->window.last_len_received) {
				//printf("stage5\n");
				sock->window.last_seq_received = seq;
				sock->window.last_len_received = buffer->data_length;
				if (buffer->data_length + sock->received_len > MAX_NETWORK_BUFFER) {
					//printf("------------------------error!!!!!!!----------------");
					//break;
				}

				if (sock->received_buf == NULL) {
					sock->received_buf = malloc(buffer->data_length);
				}
				else {
					sock->received_buf = realloc(sock->received_buf, sock->received_len + buffer->data_length);
				}
				if (!sock->received_buf)
					printf("error!\n");
				//printf("r长度:%d\n", sock->received_len + buffer->data_length);
				memcpy(sock->received_buf + sock->received_len, buffer->data, buffer->data_length);
				sock->received_len += buffer->data_length;
				free(buffer->data);
				free(buffer);
				recv_package_buffer_t* next_buffer = head;
				//while (pthread_mutex_lock(&(sock->recv_lock)) != 0);
				recv_package_buffer_t* next_next;
				while (next_buffer) {
					if (next_buffer->seq == sock->window.last_seq_received + sock->window.last_len_received) {
						sock->window.last_seq_received = next_buffer->seq;
						sock->window.last_len_received = next_buffer->data_length;
						if (next_buffer->data_length + sock->received_len > MAX_NETWORK_BUFFER) {
							//printf("------------------------error!!!!!!!----------------");
							//break;
						}
						if (sock->received_buf == NULL) {
							sock->received_buf = malloc(next_buffer->data_length);
						}
						else {
							sock->received_buf = realloc(sock->received_buf, sock->received_len + next_buffer->data_length);
						}
						//printf("r长度:%d\n", sock->received_len + next_buffer->data_length);
						if (!sock->received_buf)
							printf("error!\n");
						memcpy(sock->received_buf + sock->received_len, next_buffer->data, next_buffer->data_length);
						sock->received_len += next_buffer->data_length;
						next_next = next_buffer->next;
						free(next_buffer->data);
						free(next_buffer);
						next_buffer = next_next;
						//next_buffer->prev = NULL;
						sock->window.recv_buffer_head = next_buffer;
					}
					else
						break;
				}

				//pthread_mutex_unlock(&(sock->recv_lock));

			}
			else {
				//无序到达的数据包
				if (head == NULL) {
					sock->window.recv_buffer_head = buffer;
					sock->window.recv_buffer_head->next = NULL;
					sock->window.recv_buffer_head->prev = NULL;

				}
				else {
					recv_package_buffer_t* next_node = head;
					while (next_node) {
						if (next_node->seq > seq) {
							if (next_node->prev)
								next_node->prev->next = buffer;
							buffer->prev = next_node->prev;
							next_node->prev = buffer;
							buffer->next = next_node;
							if (next_node == head) {
								sock->window.recv_buffer_head = buffer;
							}
							break;
						}
						if (next_node->next == NULL) {
							next_node->next = buffer;
							buffer->prev = next_node;
							buffer->next = NULL;
						}
					}
				}

			}

			//if (sock->window.recv_buffer_head == NULL) {
			//	sock->window.recv_buffer_head = (recv_package_buffer_t*)malloc(sizeof(recv_package_buffer_t*));
			//}


			//recv_package_buffer_t* tail;



		}
		uint16_t rwnd = MAX_NETWORK_BUFFER-1 - sock->received_len;
		//printf("receiver_rwnd:%d\n", rwnd);
		//printf("received_长度:%d\n", sock->received_len);
		//printf("send_ack_number:%d\n", sock->window.last_seq_received + sock->window.last_len_received);
		rsp = create_packet_buf(sock->my_port, ntohs(sock->conn.sin_port), sock->window.last_ack_received + sock->window.un_acked_len, sock->window.last_seq_received + sock->window.last_len_received,
			DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, rwnd, 0, NULL, NULL, 0);
		////printf("send_ack_number!:%d\n", sock->window.last_seq_received + sock->window.last_len_received);

		sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, (struct sockaddr*)
			&(sock->conn), conn_len);
		////printf("send_ack_number!!:%d\n", sock->window.last_seq_received + sock->window.last_len_received);

		free(rsp);
		////printf("send_ack_number!!!:%d\n", sock->window.last_seq_received + sock->window.last_len_received);

		break;
	}
	////printf("end\n");
}

/*
* Param: sock - The socket used for receiving data on the connection.
* Param: flags - Signify different checks for checking on received data.
*  These checks involve no-wait, wait, and timeout.
*
* Purpose: To check for data received by the socket.
*
*/
void check_for_data(cmu_socket_t * sock, int flags) {

	char hdr[DEFAULT_HEADER_LEN];
	
	socklen_t conn_len = sizeof(sock->conn);
	ssize_t len = 0;
	uint32_t plen = 0, buf_size = 0, n = 0;
	fd_set ackFD;
	struct timeval time_out;
	time_out.tv_sec = 3;
	time_out.tv_usec = 0;
	int a = 0;

	while (pthread_mutex_lock(&(sock->recv_lock)) != 0);

	do {
		
		switch (flags) {
		case NO_FLAG:
			len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_PEEK,
				(struct sockaddr *) &(sock->conn), &conn_len);
			break;
		case TIMEOUT:
			FD_ZERO(&ackFD);
			FD_SET(sock->socket, &ackFD);
			if (select(sock->socket + 1, &ackFD, NULL, NULL, &time_out) <= 0) {
				break;
			}
		case NO_WAIT:
			len = recvfrom(sock->socket, hdr, DEFAULT_HEADER_LEN, MSG_DONTWAIT | MSG_PEEK,
				(struct sockaddr *) &(sock->conn), &conn_len);
			break;
		default:
			perror("ERROR unknown flag");
		}
		if (len >= DEFAULT_HEADER_LEN) {
			//printf("haha\n");
			plen = get_plen(hdr);
			//printf("plen:%d\n",plen);
			char* pkt = calloc(plen, sizeof(char));
			//printf("haha2\n");
			n = recvfrom(sock->socket, pkt, plen,
				NO_FLAG, (struct sockaddr *) &(sock->conn), &conn_len);
			//printf("haha4\n");
			handle_message(sock, pkt);
			//printf("hend！\n");
			//printf("a=%d\n", a);
			a = 2;
			//free(pkt);
			pkt = NULL;
			//printf("hend！\n");
			
		}
	} while (len > 0);
	pthread_mutex_unlock(&(sock->recv_lock));
}

/*
* Param: sock - The socket to use for sending data
* Param: data - The data to be sent
* Param: buf_len - the length of the data being sent
*
* Purpose: Breaks up the data into packets and sends a single
*  packet at a time.
*
* Comment: This will need to be updated for checkpoints 1,2,3
*
*/

uint32_t calculate_send_len(uint32_t buf_len, uint32_t max_dlen, uint32_t unused_window_size) {
	uint32_t min = buf_len;
	if (min > max_dlen)
		min = max_dlen;
	if (min > unused_window_size)
		min = unused_window_size;
	return min;
}

void single_send(cmu_socket_t * sock, char* data, int buf_len) {
	char* msg;
	char* data_offset = data;
	int sockfd, plen;
	size_t conn_len = sizeof(sock->conn);
	uint32_t seq;
	struct timeval now;
	struct timeval gap;

	
	if (buf_len > 0) {
		gettimeofday(&sock->window.start, NULL);
		long time_out = sock->window.RTO;
		while (buf_len != 0 || sock->window.un_acked_len != 0) {
			
			if (sock->window.send_buffer_head)
			{//超时重传

				gettimeofday(&now, NULL);
				if ((now.tv_usec - sock->window.start.tv_usec)<0) {
					gap.tv_sec = now.tv_sec - sock->window.start.tv_sec - 1;
					gap.tv_usec = 1000000 + now.tv_usec - sock->window.start.tv_usec;
				}
				else {
					gap.tv_sec = now.tv_sec - sock->window.start.tv_sec;
					gap.tv_usec = now.tv_usec - sock->window.start.tv_usec;
				}
				long time_gap = 1000000 * gap.tv_sec + gap.tv_usec;

				sock->window.RTT = (long)((sock->window.RTT)*0.875 + time_gap*0.125);//更新RTT
				sock->window.DevRTT = (long)((sock->window.DevRTT)*0.75 + (abs(time_gap - sock->window.RTT))*0.25);//更新DevRTT
				sock->window.RTO = (sock->window.RTT) + (sock->window.DevRTT) * 4;//更新RTO

				if (time_gap > time_out) {
					
					sock->window.RTO = (sock->window.RTO) * 2;//超时RTO翻倍
					gettimeofday(&sock->window.start, NULL);

					//超时的情况下
					sock->window.window_ssthresh = (sock->window.window_ssthresh) / 2;
					sock->window.window_size = sock->window.MSS;
					sock->window.dupACKcount = 0;//time out 处理
					if (sock->window.wd_state == slow_start) {
						sock->window.wd_state = slow_start;
					}
					else if (sock->window.wd_state == congestion_avoi) {
						sock->window.wd_state = slow_start;
					}
					else {
						sock->window.wd_state = slow_start;
					}

					sendto(sockfd, sock->window.send_buffer_head->package, get_plen(sock->window.send_buffer_head->package), 0, (struct sockaddr*) &(sock->conn), conn_len);
					////printf("超时！重传%d\n", sock->window.send_buffer_head->seq);
					////printf("RTO:%d\n", sock->window.RTO);
				}

			}
			if (buf_len > 0) {
				////printf("buf_len:%d\n", buf_len);

				seq = sock->window.last_ack_received + sock->window.un_acked_len;
				uint32_t window_size = sock->window.window_size;
				
				if (window_size>sock->window.rwnd)
					window_size = sock->window.rwnd;
				uint32_t unused_window_size = window_size - sock->window.un_acked_len;
				if (window_size < sock->window.un_acked_len)
					unused_window_size = 0;
				uint32_t send_len = calculate_send_len(buf_len, MAX_DLEN, unused_window_size);
				//printf("send_len:%d\n", sock->window.un_acked_len);
				////printf("test:%s\n", data);
				if (send_len > 0 ) {
					sockfd = sock->socket;

					plen = DEFAULT_HEADER_LEN + send_len;
					msg = create_packet_buf(sock->my_port, sock->their_port, seq, seq,
						DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, NULL, data_offset, send_len);
					buf_len -= send_len;
					//printf("发送seq：%d\n", seq);
					//printf("发送长度：%d\n", send_len);
					sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
					send_package_buffer_t* head = sock->window.send_buffer_head;
					send_package_buffer_t* tail;
					if (head == NULL) {
						head = (send_package_buffer_t *)calloc(1,sizeof(send_package_buffer_t));
						sock->window.send_buffer_head = head;
						head->package = msg;
						head->seq = seq;
						head->prev = NULL;
						head->next = NULL;

						sock->window.send_buffer_tail = head;

					}
					else {
						tail = (send_package_buffer_t *)calloc(1,sizeof(send_package_buffer_t));
						tail->package = msg;
						tail->seq = seq;
						tail->next = NULL;
						tail->prev = sock->window.send_buffer_tail;
						sock->window.send_buffer_tail->next = tail;
						sock->window.send_buffer_tail = tail;
					}
					sock->window.un_acked_len += send_len;
					data_offset = data_offset + send_len;
				}
				if (sock->window.rwnd == 0) {
					sockfd = sock->socket;

					plen = DEFAULT_HEADER_LEN;
					msg = create_packet_buf(sock->my_port, sock->their_port, seq, seq,
						DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, NULL, data_offset, 0);
					//printf("发送rwnd seq：%d\n", seq);
					free(msg);
					sendto(sockfd, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
					sleep(1);
				}
			}
			check_for_data(sock, NO_WAIT);


		}
	}
}

/*
* Param: in - the socket that is used for backend processing
*
* Purpose: To poll in the background for sending and receiving data to
*  the other side.
*
*/
void* begin_backend(void * in) {
	cmu_socket_t * dst = (cmu_socket_t *)in;
	int death, buf_len, send_signal;
	char* data;

	while (TRUE) {
		while (pthread_mutex_lock(&(dst->death_lock)) != 0);
		death = dst->dying;
		pthread_mutex_unlock(&(dst->death_lock));


		while (pthread_mutex_lock(&(dst->send_lock)) != 0);
		buf_len = dst->sending_len;

		if (death && buf_len == 0)
			break;

		if (buf_len > 0) {
			data = malloc(buf_len);
			memcpy(data, dst->sending_buf, buf_len);
			////printf("data_to_send:%s\n", data);
			dst->sending_len = 0;
			free(dst->sending_buf);
			dst->sending_buf = NULL;
			pthread_mutex_unlock(&(dst->send_lock));
			single_send(dst, data, buf_len);
			free(data);
		}
		else
			pthread_mutex_unlock(&(dst->send_lock));
		check_for_data(dst, NO_WAIT);

		while (pthread_mutex_lock(&(dst->recv_lock)) != 0);

		if (dst->received_len > 0)
			send_signal = TRUE;
		else
			send_signal = FALSE;
		pthread_mutex_unlock(&(dst->recv_lock));

		if (send_signal) {
			pthread_cond_signal(&(dst->wait_cond));
		}
	}


	pthread_exit(NULL);
	return NULL;
}
