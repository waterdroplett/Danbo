// pci.h
#ifndef PCI_H
#define PCI_H

#include <stdint.h>

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off);
int pci_find(uint16_t vendor, uint16_t device, uint8_t *bus, uint8_t *dev, uint8_t *fn);

#endif