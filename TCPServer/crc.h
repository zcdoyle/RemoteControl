#ifndef __CRC__
#define __CRC__

#include <stdint.h>

unsigned short CRC16(unsigned char *puchMsg, unsigned short usDataLen);
unsigned short CRC32(unsigned char *puchMsg, unsigned short usDataLen);
#endif
