// pci.c
#include "pci.h"
#include "io.h"

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                ((uint32_t)fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}

int pci_find(uint16_t vendor, uint16_t device, uint8_t *bus, uint8_t *dev, uint8_t *fn) {
    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            uint32_t id = pci_read32(b, d, 0, 0);
            if (id == 0xFFFFFFFFu) continue;           // no device here
            if ((id & 0xFFFF) == vendor && (id >> 16) == device) {
                *bus = b; *dev = d; *fn = 0;
                return 1;
            }
        }
    }
    return 0;
}