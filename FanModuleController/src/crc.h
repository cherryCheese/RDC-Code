/*
 * crc.h
 *
 * Created: 11/9/2020 4:11:31 PM
 *  Author: E1210640
 */ 


#ifndef CRC_H_
#define CRC_H_

uint16_t crc16(uint16_t crc, const uint8_t *buf, uint32_t len, uint16_t polynomial);

#endif /* CRC_H_ */