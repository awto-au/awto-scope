#ifndef A2K_UI_SOFTKEY_H
#define A2K_UI_SOFTKEY_H

#include <stdint.h>

#define A2K_SOFTKEYS 4

void a2k_softkey_set(uint8_t idx, const char *label);
void a2k_softkey_clear_all(void);
const char *a2k_softkey_get(uint8_t idx);
void a2k_softkey_render(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

#endif /* A2K_UI_SOFTKEY_H */
