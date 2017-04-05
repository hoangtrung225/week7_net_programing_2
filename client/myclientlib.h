struct packet_header {
	char opcode;
	short length;
};

enum header{
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

int header_wraper(char* buffer, int opt_code, int len_of_payload);