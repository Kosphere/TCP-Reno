#ifndef _CMU_BACK_H_
#define _CMU_BACK_H_
#include "cmu_tcp.h"
#include "global.h"
#include "cmu_packet.h"

int check_ack(cmu_socket_t * dst, uint32_t seq);
void check_for_data(cmu_socket_t * dst, int flags);
void * begin_backend(void * in);

typedef struct{
      int buflen;
      cmu_packet_t** pkbuf;
      int *send_sign;//鐢ㄤ簬瀛樻斁鏁版嵁鍖呮槸鍚﹁绗竴娆″彂閫佽繃鐨勪俊鎭?
      int *acktimes;//鐢ㄤ簬瀛樻斁ACK鏀跺埌鐨勬鏁?
      int *ack;//鐢ㄤ簬瀛樻斁搴旇鏀跺埌鐨刟ck鐨勫€?
      int sendbase;
    }tem_window;
#endif
