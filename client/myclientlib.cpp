#include "stdafx.h"
#include "myclientlib.h"
#include "stdio.h"
#include "string.h"


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