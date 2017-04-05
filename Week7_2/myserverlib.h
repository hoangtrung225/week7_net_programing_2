#include <stdio.h>
#define BUFFSIZE 512

struct packet_header {
	char opcode;
	short length;
};

struct client_info_struct {
	int state;
	int encrypt_key;
	char* client_buffer;
	SOCKET client_fd;
	FILE * file_ptr;
	int is_active;
	int ok_to_write;
};

enum state{
	WAITING = 0,
	RECEIVE_FILE,
	RETURN_FILE
};

enum header {
	ENCRYPT_FILE = 0,
	DECRYPT_FILE = 1,
	TRANSMIT_FILE = 2,
	TRANSMIT_ERROR = 3
};

//signal replace leght of transmit with value bigger than BUFFSIZE
enum signal {
	SIGNAL_ACK = 2205,
	SIGNAL_END = 0
};

int receive_data(SOCKET s, char* buffer, int size, int flags);
int send_data(SOCKET s, char* buffer, int size, int flags);
int process_data(struct client_info_struct* client_info);

char* ceasar_cipher(char*, int);
char* ceasar_decipher(char*, int);
int file_encrypt_handler(SOCKET*, char*, int);
int header_wraper(char* buffer, int opt_code, int len_of_payload);