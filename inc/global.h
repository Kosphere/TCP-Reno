#include "grading.h"

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define EXIT_SUCCESS 0
#define EXIT_ERROR -1
#define EXIT_FAILURE 1

#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2

#define TRUE 1
#define FALSE 0


typedef struct send_package_buffer_t send_package_buffer_t;
typedef struct recv_package_buffer_t recv_package_buffer_t;
struct send_package_buffer_t {
	char* package;
	uint32_t seq;
	send_package_buffer_t* next;
	send_package_buffer_t* prev;

} ;

struct recv_package_buffer_t {
	char* data;
	uint32_t seq;
	uint32_t data_length;
	recv_package_buffer_t* next;
	recv_package_buffer_t* prev;

};


typedef struct {
	uint32_t last_seq_received;
	uint32_t last_ack_received;
	send_package_buffer_t* send_buffer_head;
	send_package_buffer_t* send_buffer_tail;
	recv_package_buffer_t* recv_buffer_head;
	recv_package_buffer_t* recv_buffer_tail;
	uint32_t last_len_received;
	uint32_t window_size;
	uint32_t un_acked_len;
	struct timeval start;
	uint32_t window_ssthresh;
	uint32_t dupACKcount;
	long RTT;
	long DevRTT;
	long RTO;//time_out
	uint32_t MSS;
	enum FIN_STATE{slow_start=1,congestion_avoi,fast_recovery}wd_state;
	uint32_t rwnd;
	int is_timing;

	pthread_mutex_t ack_lock;


} window_t;


typedef struct {
	int socket;   
	pthread_t thread_id;
	uint16_t my_port;
	uint16_t their_port;
	struct sockaddr_in conn;
	char* received_buf;
	uint32_t received_len;
	pthread_mutex_t recv_lock;
	pthread_cond_t wait_cond;
	char* sending_buf;
	int sending_len;
	int type;
	pthread_mutex_t send_lock;
	int dying;
	pthread_mutex_t death_lock;
	window_t window;
	enum STATE{INIT=1,SYN_SENT,SYN_RCVD,CONN,FIN_WAIT1,FIN_WAIT2,CLOSE_WAIT,LAST_ACK,TIME_WAIT,CLOSED}state;
} cmu_socket_t;

#endif