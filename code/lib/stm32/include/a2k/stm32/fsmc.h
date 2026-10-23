#ifndef A2K_STM32_FSMC_H
#define A2K_STM32_FSMC_H

#include <stdint.h>

extern volatile uint16_t *a2k_fsmc_cmd;
extern volatile uint16_t *a2k_fsmc_data;

void a2k_fsmc_init(void);
void a2k_fsmc_cmd_write(uint8_t cmd);
void a2k_fsmc_data_write(uint16_t dat);
uint16_t a2k_fsmc_data_read(void);

#endif /* A2K_STM32_FSMC_H */
