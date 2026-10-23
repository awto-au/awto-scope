#ifndef A2K_STM32_DMA_FILL_H
#define A2K_STM32_DMA_FILL_H

#include <stdint.h>

void a2k_dma_fill(volatile uint16_t *dest, uint16_t colour, uint32_t count);

#endif /* A2K_STM32_DMA_FILL_H */
