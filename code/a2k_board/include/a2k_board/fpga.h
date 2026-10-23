#ifndef A2K_BOARD_FPGA_H
#define A2K_BOARD_FPGA_H

#include <stdint.h>
#include "a2k_board/drv.h"

int              fpga_init(void);
const a2k_drv_t *fpga_status(void);
void             fpga_write_reg(uint8_t addr, uint8_t data);
int              fpga_read_reg(uint8_t addr, uint8_t *out);

#endif /* A2K_BOARD_FPGA_H */
