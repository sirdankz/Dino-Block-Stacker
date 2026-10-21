#ifndef TA_ROM_SHA256_H
#define TA_ROM_SHA256_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void ta_rom_sha256(const uint8_t* data,size_t bytes,uint8_t digest[32]);
bool ta_r60_rom_sha256(const uint8_t* data,size_t bytes);
#endif
