/*
 * can_bus.h
 * CAN总线 (PB8/PB9)
 */

#ifndef __CAN_BUS_H
#define __CAN_BUS_H

#include <stdint.h>

void CAN_Init(void);
void CAN_Send(uint32_t id, uint8_t *data, uint8_t len);

#endif /* __CAN_BUS_H */
