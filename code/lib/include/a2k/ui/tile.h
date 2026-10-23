#ifndef A2K_UI_TILE_H
#define A2K_UI_TILE_H

#include <stdint.h>
#include "a2k/ui/channel.h"

typedef enum { TILE_LARGE=0, TILE_MEDIUM, TILE_SMALL } a2k_tile_size_t;

a2k_tile_size_t a2k_tile_size(uint16_t tile_h);
void a2k_tile_render(a2k_channel_t *ch, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

#endif /* A2K_UI_TILE_H */
