#include "stdafx.h"
#include "string.h"
#include "winsock2.h"
#include "stdio.h"
#include "myserverlib.h"

#define FILETEMP "filetmp"
#define BUFFSIZE 512

#pragma comment(lib, "Ws2_32.lib")


char* ceasar_cipher(char* string, int key) {
	int len_of_string = strlen(string);
	if (key > 0)
		key = key % ('Z' - 'A');
	else
		key = -(-key % ('Z' - 'A'));
	for (int i = 0; i < len_of_string; i++)
		string[i] = string[i] + key;
	return string;
}

char* ceasar_decipher(char* string, int key) {
	return ceasar_cipher(string, -key);
}

int file_encrypt_handler(SOCKET* socket, char* buffer, int key) {
	struct packet_header packet_header;
	SOCKET server_sock = *socket;
	FILE* file = fopen(FILETEMP, "w+");
	int byte_transmit = 0;

	//encrypt while tranfer file
	do {
		if ((byte_transmit = recv(server_sock, buffer, BUFFSIZE, 0)) < 0) {
			printf("Error: Client close conection while transmite data\n");
			break;
		}
		//copy header
		memcpy(&packet_header, buffer, sizeof(packet_header));

		if (packet_header.opcode == 2) {
			//end of file
			if (packet_header.length == 0) {
				break;
			}
			char* payload = buffer + sizeof(packet_header);
			payload[packet_header.length] = '\0';
			payload = ceasar_cipher(payload, key);
			fwrite(payload, sizeof(char), byte_transmit - sizeof(packet_header), file);
		}
		else {

			// receive other operation than transmit while transfer file
			header_wraper(buffer, 3, 1);
			send(server_sock, buffer, sizeof(packet_header), 0);
			continue;
		}
	} while (packet_header.length > 0);

	//finished writing temporary enrypted file, read file from begining and send back to client
	fseek(file, 0, SEEK_SET);
	int byteread;
	while ((byteread = fread(buffer + sizeof(packet_header), sizeof(char), BUFFSIZE - sizeof(packet_header), file)) > 0) {
		header_wraper(buffer, 2, byteread);
		if (send(server_sock, buffer, byteread, 0) < 0)
			break;

		//receive ACK
		if (recv(server_sock, buffer, BUFFSIZE, 0) < 0) break;
		memcpy(&packet_header, buffer, sizeof packet_header);
		if (packet_header.opcode == 2 && packet_header.length == 2205) continue;
		else {
			printf("Server not receive ACK! some thing must go wrong\n");
			break;
		}
	}
	fclose(file);

	//send end of file
	header_wraper(buffer, 2, 0);
	send(server_sock, buffer, sizeof packet_header, 0);
	return 0;
}

int header_wraper(char* buffer, int opt_code, int len_of_payload) {
	if (buffer == NULL) return -1;
	if (opt_code < 0 || opt_code > 3) return -1;
	if (len_of_payload < 0) return -1;
	struct packet_header tmp;
	tmp.opcode = opt_code;
	tmp.length = len_of_payload;
	memcpy(buffer, &tmp, sizeof(tmp));
	return 0;
}

int receive_data(SOCKET s, char* buffer, int size, int flags) {
	int n = recv(s, buffer, size, flags);
	if (n <= 0)
		printf("Error! cannot receive message.\n");
	else
		buffer[n] = 0;
	return n;
}

int send_data(SOCKET s, char* buffer, int size, int flags) {
	int n = send(s, buffer, size, flags);
	if (n <= 0)
		printf("Error! cannot send message.\n");
	return n;
}

int process_data(struct client_info_struct* client_info) {

	//copy header
	packet_header packet_header;
	memcpy(&packet_header, client_info->client_buffer, sizeof(packet_header));
	int key;
	switch (packet_header.opcode)
	{
	case ENCRYPT_FILE:
		//server send ok
		printf("Server ready to encrypt file\n");

		//get key from user payload
		memcpy(&key, client_info->client_buffer + sizeof(packet_header), sizeof(int));
		client_info->encrypt_key = key;

		client_info->file_ptr = fopen(FILETEMP, "w+");
		//cant not open file
		if (client_info->file_ptr == NULL) {
			printf("Server fail to open new file for encryption\n");
			client_info->file_ptr = NULL;
			client_info->state = WAITING;
			header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
			return -1;
		}

		header_wraper(client_info->client_buffer, TRANSMIT_FILE, SIGNAL_ACK);
		client_info->state = RECEIVE_FILE;

		return 0;
	case DECRYPT_FILE:
		//server send ok
		printf("Server ready to decrypt file\n");

		//get key from user payload
		memcpy(&key, client_info->client_buffer + sizeof(packet_header), sizeof(int));
		client_info->encrypt_key = -key;

		client_info->file_ptr = fopen(FILETEMP, "w+");

		//cant not open file
		if (client_info->file_ptr == NULL) {
			printf("Server fail to open new file for decryption\n");
			client_info->file_ptr = NULL;
			client_info->state = WAITING;
			header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
			return -1;
		}

		header_wraper(client_info->client_buffer, TRANSMIT_FILE, SIGNAL_ACK);
		client_info->state = RECEIVE_FILE;

		return 0;
	case TRANSMIT_FILE:
		//get payload from buffer
		char* payload = client_info->client_buffer + sizeof(packet_header);

		FILE* file = client_info->file_ptr;
		if (file == NULL) {
			printf("Error: server fail to write to temporary file.\n");
			header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
			return -1;
		}

		if (client_info->state == RECEIVE_FILE) {
			//state is receiving file and get SIGNAL_END mean end of transmit file 
			if (packet_header.length == SIGNAL_END) {
				//change client state
				client_info->state = RETURN_FILE;
				fseek(file, 0, SEEK_SET);
				// read file write to buffer
				int readbyte = fread(payload, sizeof(char), BUFFSIZE - sizeof(packet_header), file);
				if (readbyte > 0) {
					// wrap respond header
					header_wraper(client_info->client_buffer, TRANSMIT_FILE, readbyte);
					return 0;
				}
				else {
					//end of file
					if (feof(file)) {
						header_wraper(client_info->client_buffer, TRANSMIT_FILE, SIGNAL_END);
						return 0;
					}
					else {
						printf("Receive error\n");
						header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
						return -1;
					}
				}
			}
			else if (packet_header.length > 0) {
				//receive size length buffer
				ceasar_cipher(payload, client_info->encrypt_key);
				int readbyte = fwrite(payload, sizeof(char), BUFFSIZE - sizeof(packet_header), file);
				if (readbyte > 0) {
					// wrap respond header
					header_wraper(client_info->client_buffer, TRANSMIT_FILE, readbyte);
					return 0;
				}
				else{
					printf("Receive error\n");
					header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
					return -1;
				}
			}
		}
		else if (client_info->state == RETURN_FILE) {
			if (packet_header.length > 0) {
				//receive size length buffer
				int readbyte = fread(payload, sizeof(char), BUFFSIZE - sizeof(packet_header), file);
				if (readbyte > 0) {
					// wrap respond header
					header_wraper(client_info->client_buffer, TRANSMIT_FILE, readbyte);
					return 0;
				}
				else {
					//end of file
					if (feof(file)) {
						header_wraper(client_info->client_buffer, TRANSMIT_FILE, SIGNAL_END);
						fclose(file);
						client_info->file_ptr = NULL;
						return 0;
					}
					else {
						printf("Receive error\n");
						header_wraper(client_info->client_buffer, TRANSMIT_ERROR, SIGNAL_ACK);
						return -1;
					}
				}
			}
		}
	}//end of switch
}