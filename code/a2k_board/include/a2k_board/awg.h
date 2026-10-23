#ifndef A2K_BOARD_AWG_H
#define A2K_BOARD_AWG_H

#include <stdint.h>
#include <stdbool.h>
#include "a2k_board/drv.h"

int              awg_init(void);
const a2k_drv_t *awg_status(void);
uint32_t         awg_freq_word(uint32_t freq_hz);
void             awg_set_freq(uint32_t freq_hz);
void             awg_enable(bool on);

#endif /* A2K_BOARD_AWG_H */
