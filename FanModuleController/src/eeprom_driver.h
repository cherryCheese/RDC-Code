/*
 * eeprom.h
 *
 * Created: 12/6/2020 10:53:03 AM
 *  Author: E1210640
 */ 

/*EEPROM Byte mapping

byte 0 = Device P/N (high byte)
byte 1 = Device P/N
byte 2 = Device P/N
byte 3 = Device P/N
byte 4 = Device P/N
byte 5 = Device P/N
byte 6 = Device P/N
byte 7 = Device P/N (low byte)

byte 8 = Device S/N (high byte)
byte 9 = Device S/N
byte 10 = Device S/N
byte 11 = Device S/N
byte 12 = Device S/N
byte 13 = Device S/N
byte 14 = Device S/N
byte 15 = Device S/N
byte 16 = Device S/N
byte 17 = Device S/N
byte 18 = Device S/N
byte 19 = Device S/N (low byte)

byte 20 = Controller P/N (high byte)
byte 21 = Controller P/N
byte 22 = Controller P/N
byte 23 = Controller P/N
byte 24 = Controller P/N
byte 25 = Controller P/N
byte 26 = Controller P/N
byte 27 = Controller P/N (low byte)

byte 28 = Controller S/N (high byte)
byte 99 = Controller S/N
byte 30 = Controller S/N
byte 31 = Controller S/N
byte 32 = Controller S/N
byte 33 = Controller S/N
byte 34 = Controller S/N
byte 35 = Controller S/N
byte 36 = Controller S/N
byte 37 = Controller S/N
byte 38 = Controller S/N
byte 39 = Controller S/N (low byte)

byte 40 to 60 = reserved 
*/


#ifndef EEPROM_H_
#define EEPROM_H_

void eeprom_init(void);
int eeprom_read(uint8_t *buf, int offset, int len);
int eeprom_write(const uint8_t *buf, int offset, int len);

#endif /* EEPROM_H_ */