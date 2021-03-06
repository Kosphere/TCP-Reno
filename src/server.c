#include "../inc/cmu_tcp.h"

/*
* Param: sock - used for reading and writing to a connection
*
* Purpose: To provide some simple test cases and demonstrate how
*  the sockets will be used.
*
*/
void functionality(cmu_socket_t  * sock) {
	char buf[9898];
	FILE *fp;
	int n;
	//sleep(30);
	sleep(3);
	n = cmu_read(sock, buf, 200, NO_FLAG);
	printf("R: %s\n", buf);
	printf("N: %d\n", n);
	cmu_write(sock, "hi their", 9);
	cmu_read(sock, buf, 200, NO_FLAG);
	cmu_write(sock, "hi there", 9);

	sleep(3);
	fp = fopen("./test/test.dat", "w+");
	for (int i = 0; i < 64000; i++) {
		//sleep(1);
		n = cmu_read(sock, buf, 9898, NO_FLAG);
		printf("N: %d i:%d\n", n,i);
		
		fwrite(buf, 1, n, fp);
		
	}

}


/*
* Param: argc - count of command line arguments provided
* Param: argv - values of command line arguments provided
*
* Purpose: To provide a sample listener for the TCP connection.
*
*/
int main(int argc, char **argv) {
	int portno;
	char *serverip;
	char *serverport;
	cmu_socket_t socket;

	serverip = getenv("server15441");
	if (serverip);
	else {
		serverip = "10.0.0.1";
	}

	serverport = getenv("serverport15441");
	if (serverport);
	else {
		serverport = "15441";
	}
	portno = (unsigned short)atoi(serverport);


	if (cmu_socket(&socket, TCP_LISTENER, portno, serverip) < 0)
		exit(EXIT_FAILURE);

	functionality(&socket);

	if (cmu_close(&socket) < 0)
		exit(EXIT_FAILURE);
	return EXIT_SUCCESS;
}
