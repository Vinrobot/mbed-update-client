#pragma once

#include "mbed.h"

namespace update_client {

// FlashUpdater is an extension of FlashIAP for dealing with application updates stored on the internal Flash

class FlashUpdater :
    public FlashIAP {
public:
    FlashUpdater();

    // read a page from a specified address and update the address for reading from the next page
    int32_t readPage(uint32_t pageSize, char *readPageBuffer, uint32_t &addr);
    // write a page to a specified address and update the parameters for writing to the next page
    int32_t writePage(uint32_t pageSize, char *writePageBuffer, char *readPageBuffer,
                      uint32_t &addr, bool &sectorErased, size_t &pagesFlashed, uint32_t &nextSectorAddress);
    // returns the address passed as parameter aligned to the flash sector
    uint32_t alignAddressToSector(uint32_t address, bool roundDown);
};

} // namespace update_client
