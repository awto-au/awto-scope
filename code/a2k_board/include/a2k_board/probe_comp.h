#ifndef A2K_BOARD_PROBE_COMP_H
#define A2K_BOARD_PROBE_COMP_H

#include "a2k_board/drv.h"

int              probe_comp_init(void);
const a2k_drv_t *probe_comp_status(void);
void             probe_comp_start(void);
void             probe_comp_stop(void);

#endif /* A2K_BOARD_PROBE_COMP_H */
