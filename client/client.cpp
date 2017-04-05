#include "stdafx.h"
#include "stdio.h"
#include "conio.h"
#include "string.h"
#include "ws2tcpip.h"
#include "winsock2.h"
#include "process.h"
#include "ctype.h"
#include "myclientlib.h"

#pragma comment(lib, "Ws2_32.lib")

#define BUFF_SIZE 512
#define DEFAULT_PORT 5500
#define DEFAULT_ADDR "127.0.0.1"
int main(int argc, char* argv[])
{
	SOCKET clientSocket;
	WSADATA wsaData;

	//Step 1: Inittiate WinSock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Version is not supported!");
		_getch();
		return 0;
	}

	//Step 2: Create socket	
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket < 0) {
		printf("Error! Cannot create socket.");
		_getch();
		return 0;
	}

	//Step 3: Specify server address
	int port;
	char ipAddr[16];
	port = DEFAULT_PORT;
	strcpy(ipAddr, DEFAULT_ADDR);
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0)	port = atoi(argv[++i]);
		if (strcmp(argv[i], "-a") == 0)	strcpy(ipAddr, argv[++i]);
	}


	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(ipAddr);
	printf("Connecting to %s:%d\n", ipAddr, port);

	//Step 4: Request to connect server
	if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)))
		printf("Cannot connect to server.");

	//Step 5: Communicate with server
	struct packet_header packet_header;
	char buffer[BUFF_SIZE + 1];
	int key, byteread;
	char filename[64];
	FILE * file;
	char* payload = buffer + sizeof(packet_header);
	while (1) {

		printf("SELECT ONE ACTION BELOW: \n");
		printf("0: encrypt file.\n");
		printf("1: decrypt file.\n");
		printf("SELECTED OPTION: ");
		int action_code;
		scanf("%d", &action_code);
		switch (action_code)
		{
		case ENCRYPT_FILE:
			printf("Enter key to encrypt file: ");
			scanf("%d", &key);

			//send option selected for encrypt/decrypt
			header_wraper(buffer, ENCRYPT_FILE, sizeof key);
			memcpy(payload, &key, sizeof(key));
			send(clientSocket, buffer, BUFF_SIZE, 0);

			//receive server respond restart on error
			recv(clientSocket, buffer, BUFF_SIZE, 0);
			memcpy(&packet_header, buffer, sizeof packet_header);
			if (packet_header.opcode != TRANSMIT_FILE) continue;

			printf("Server say ok! Enter file location to encrypt: ");
			scanf("%s", filename);
			file = fopen(filename, "r");
			if (file == NULL) {
				printf("Error can not open file\n");
				continue;
			}
			//send file
			while ((byteread = fread(payload, sizeof(char), BUFF_SIZE - sizeof packet_header, file)) > 0) {
				header_wraper(buffer, TRANSMIT_FILE, byteread);
				if (send(clientSocket, buffer, sizeof packet_header + byteread, 0) < 0) {
					printf("Error connection lost!\n");
					break;
				}
				//recv fail 
				if (recv(clientSocket, buffer, BUFF_SIZE, 0) < 0) {
					printf("Error connection lost!\n");
					break;
				}
				memcpy(&packet_header, buffer, sizeof packet_header);
				//receive ack report
				if (packet_header.opcode == TRANSMIT_FILE)	continue;
				//some thing else happen is error
				else break;
			}

			//send end of file 
			header_wraper(buffer, TRANSMIT_FILE, SIGNAL_END);
			send(clientSocket, buffer, sizeof packet_header, 0);
			fclose(file);
			//read from server
			printf("\n----------------------------Start of encrypton--------------------------------\n");

			while (recv(clientSocket, buffer, BUFF_SIZE, 0) > 0) {
				memcpy(&packet_header, buffer, sizeof(packet_header));
				if (packet_header.opcode == TRANSMIT_FILE) {
					if (packet_header.length == SIGNAL_END) {
						printf("\n----------------------------End of encrypton enter to continue----------------------------\n");
						break;
					}
					payload[packet_header.length] = '\0';
					printf("%s", payload);

					//send ACK
					header_wraper(buffer, TRANSMIT_FILE, SIGNAL_ACK);
					if (send(clientSocket, buffer, sizeof packet_header, 0) < 0) break;
				}
				if (packet_header.opcode == 3)
				{
					printf("Server said there is something wrong! Enter to do it again :D\n");
					break;
				}
				//error code handler go here
			}
			_getch();
			system("cls");
			continue;
		case DECRYPT_FILE:
			printf("Enter key to decrypt file: ");
			scanf_s("%d", &key);

			//send selected option
			header_wraper(buffer, 1, sizeof key);
			memcpy(payload, &key, sizeof(key));
			send(clientSocket, buffer, BUFF_SIZE, 0);

			//receive server respond restart on error
			recv(clientSocket, buffer, BUFF_SIZE, 0);
			memcpy(&packet_header, buffer, sizeof packet_header);
			if (packet_header.opcode != 2) continue;

			printf("Enter file location to decrypt: ");
			scanf("%s", filename);
			file = fopen(filename, "r");
			if (file == NULL) {
				printf("Error can not open file\n");
				continue;
			}
			//read file and send to server
			while ((byteread = fread(payload, sizeof(char), BUFF_SIZE - sizeof packet_header, file)) > 0) {
				header_wraper(buffer, 2, byteread);
				if (send(clientSocket, buffer, sizeof packet_header + byteread, 0) < 0) {
					printf("Error connection lost!\n");
					break;
				}
				//recv fail 
				if (recv(clientSocket, buffer, BUFF_SIZE, 0) < 0) {
					printf("Error connection lost!\n");
					break;
				}
				memcpy(&packet_header, buffer, sizeof packet_header);
				//receive ack report
				if (packet_header.opcode == TRANSMIT_FILE)	continue;
				//some thing else happen is error
				else break;
			}

			//send end of file 
			header_wraper(buffer, 2, 0);
			send(clientSocket, buffer, sizeof packet_header, 0);
			fclose(file);
			printf("\n----------------------------Start of decrypton----------------------------\n");

			//read from server
			while (recv(clientSocket, buffer, BUFF_SIZE, 0) > 0) {
				memcpy(&packet_header, buffer, sizeof(packet_header));
				if (packet_header.opcode == 2) {
					if (packet_header.length == 0) {
						printf("\n----------------------------End of decrypton enter to continue----------------------------\n");
						break;
					}
					payload[packet_header.length] = '\0';
					printf("%s", payload);

					//send ACK
					header_wraper(buffer, 2, 2205);
					if (send(clientSocket, buffer, sizeof packet_header, 0) < 0) break;
				}
				if (packet_header.opcode == 3) {
					printf("Server said there is some thing bad happen! Enter to do it again\n");
					continue;
				}
			}
			_getch();
			system("cls");
			continue;
		default:
			printf("selection is not valid\n");
			continue;
		}//end of switch
		if (recv(clientSocket, buffer, BUFF_SIZE, 0) < 0) {
			printf("Error! connection with server is lost.\n");
			break;
		}
		memcpy(&packet_header, buffer, sizeof(packet_header));
		printf("Server send optcode %d", (int)packet_header.opcode);
		//Error happen
		if (packet_header.opcode == 3)
			continue;

	}

	WSACleanup();
	_getch();
	return 0;
}