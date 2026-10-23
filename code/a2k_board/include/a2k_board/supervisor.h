#ifndef A2K_BOARD_SUPERVISOR_H
#define A2K_BOARD_SUPERVISOR_H
#include <stdbool.h>
#include <stdint.h>

enum a2k_msg_type {
    MSG_ERROR = 0,
    MSG_KEYPAD,
    MSG_USB_CMD,
    MSG_ESP_CMD,
    MSG_CAPTURE_DONE,
    MSG_DMM_READING,
    MSG_BATTERY_LOW,
};

struct a2k_msg {
    enum a2k_msg_type type;
    uint32_t          arg;
};

extern bool g_is_renode;

void supervisor_init(void);
void supervisor_run(void);
int  supervisor_post(enum a2k_msg_type type, uint32_t arg);
bool a2k_is_renode(void);
#endif
