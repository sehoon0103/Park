#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart.h"
#include "protocol.h"

uint8_t g_slave_id = 0x01;
uint8_t g_last_slave_id = 0x00;
uint8_t reg[256] = {0};

static int8_t hex_char_to_val(char c) {
	if (c >= '0' && c <= '9') return (int8_t)(c - '0');
	if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
	return -1;
}

static uint8_t hex_ascii_to_byte(const char* in) {
	int8_t hi = hex_char_to_val(in[0]);
	int8_t lo = hex_char_to_val(in[1]);
	if (hi < 0 || lo < 0) return 0;
	return (uint8_t)((hi << 4) | lo);
}

void byte_to_hex_ascii(uint8_t val, char* out) {
	const char hex[] = "0123456789ABCDEF";
	out[0] = hex[(val >> 4) & 0x0F];
	out[1] = hex[val & 0x0F];
}

// ASCII 문자 합산 체크섬
static uint8_t calc_checksum_ascii(const char* ascii, uint8_t len) {
	uint8_t sum = 0;
	for (uint8_t i = 0; i < len; i++) {
		sum += (uint8_t)ascii[i];
	}
	return sum;
}

static void build_ascii_frame(const ProtocolData* data, char* out_buf, uint8_t is_resp) {
	uint8_t i = 0;
	out_buf[i++] = is_resp ? ':' : '$';

	byte_to_hex_ascii(data->slave_id, &out_buf[i]); i += 2;
	out_buf[i++] = data->command;
	byte_to_hex_ascii(data->address, &out_buf[i]); i += 2;

	uint8_t has_data = 0;
	if ((out_buf[0] == '$' && data->command == 'W') ||
	(out_buf[0] == ':' && data->command == 'R')) {
		has_data = 1;
		byte_to_hex_ascii(data->data, &out_buf[i]); i += 2;
	}

	uint8_t checksum_len = has_data ? 7 : 5;
	char ascii_part[8];
	memcpy(ascii_part, &out_buf[1], checksum_len);
	ascii_part[checksum_len] = '\0';

	uint8_t chk = calc_checksum_ascii(ascii_part, checksum_len);
	byte_to_hex_ascii(chk, &out_buf[i]); i += 2;

	out_buf[i++] = '\n';
	out_buf[i] = '\0';
}

static uint8_t parse_ascii_frame(const char* buf, ProtocolData* out_data) {
	if (!buf || !out_data) return 0;

	uint8_t len = (uint8_t)strlen(buf);
	if (len < 9) return 0;
	if ((buf[0] != '$' && buf[0] != ':')) return 0;
	if (buf[len - 1] != '\n') return 0;

	char cmd = buf[3];
	if (cmd != 'W' && cmd != 'R') return 0;

	uint8_t has_data = 0;
	if ((buf[0] == '$' && cmd == 'W' && len == 11) ||
	(buf[0] == ':' && cmd == 'R' && len == 11)) {
		has_data = 1;
	} else if ((buf[0] == '$' && cmd == 'R' && len == 9) ||
	(buf[0] == ':' && cmd == 'W' && len == 9)) {
		has_data = 0;
		} else {
		return 0;
	}

	uint8_t ascii_len = has_data ? 7 : 5;
	char ascii_part[8];
	memcpy(ascii_part, &buf[1], ascii_len);
	ascii_part[ascii_len] = '\0';

	uint8_t checksum_calc = calc_checksum_ascii(ascii_part, ascii_len);
	uint8_t checksum_recv = hex_ascii_to_byte(&buf[ascii_len + 1]);
	if (checksum_calc != checksum_recv) return 0;

	out_data->slave_id = hex_ascii_to_byte(&buf[1]);
	out_data->command = cmd;
	out_data->address = hex_ascii_to_byte(&buf[4]);
	out_data->data = has_data ? hex_ascii_to_byte(&buf[6]) : 0;

	return 1;
}

void master_write(uint8_t slave_id, uint8_t address, uint8_t data) {
	ProtocolData tx = { slave_id, 'W', address, data };
	char frame[13];
	build_ascii_frame(&tx, frame, 0);
	g_last_slave_id = slave_id;
	uart_print(frame);
}

void master_read(uint8_t slave_id, uint8_t address) {
	ProtocolData tx = { slave_id, 'R', address, 0 };
	char frame[13];
	build_ascii_frame(&tx, frame, 0);
	g_last_slave_id = slave_id;
	uart_print(frame);
}

uint8_t master_receive(const char* frame_in, ProtocolData* out_data) {
	if (!frame_in || !out_data) return 0;
	if (!parse_ascii_frame(frame_in, out_data)) return 0;
	if (out_data->slave_id != g_last_slave_id) return 0;
	if (frame_in[0] != ':') return 0;
	return 1;
}

void slave_receive_and_respond(const char* frame_in) {
	ProtocolData rx;
	if (!parse_ascii_frame(frame_in, &rx)) return;
	if (rx.slave_id != g_slave_id) return;

	if (rx.command == 'W') {
		reg[rx.address] = rx.data;
		ProtocolData ack = { rx.slave_id, 'W', rx.address, 0 };
		char reply[13];
		build_ascii_frame(&ack, reply, 1);
		uart_print(reply);
	}
	else if (rx.command == 'R') {
		uint8_t data = reg[rx.address];
		ProtocolData resp = { rx.slave_id, 'R', rx.address, data };
		char reply[13];
		build_ascii_frame(&resp, reply, 1);
		uart_print(reply);
	}
}
