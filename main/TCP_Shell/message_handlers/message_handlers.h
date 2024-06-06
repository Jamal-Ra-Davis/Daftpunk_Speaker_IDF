#pragma once
#include <stdint.h>
#include <message_ids.h>

int handle_shell_message(message_id_t id, tcp_message_t *msg, tcp_message_t *resp, bool *print_message);