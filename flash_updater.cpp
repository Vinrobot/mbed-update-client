#include "flash_updater.hpp"
#include "uc_error_codes.hpp"

#include "mbed_trace.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "FlashUpdater"
#endif // MBED_CONF_MBED_TRACE_ENABLE

namespace update_client {

FlashUpdater::FlashUpdater()
{

}

int32_t FlashUpdater::readPage(uint32_t pageSize, char *readPageBuffer, uint32_t &addr)
{
    //tr_debug(" Reading page of size %d at address 0x%08x", pageSize, addr);
    int32_t err = read(readPageBuffer, addr, pageSize);
    if (0 != err) {
        tr_error("Flash read failed: %d", err);
        return err;
    }
    // update address
    addr += pageSize;

    return err;
}

int32_t FlashUpdater::writePage(uint32_t pageSize, char *writePageBuffer, char *readPageBuffer,
                                uint32_t &addr, bool &sectorErased, size_t &pagesFlashed, uint32_t &nextSectorAddress)
{
    //tr_debug(" Writing page of size %d at address 0x%08x", pageSize, addr);
    int32_t err = UC_ERR_NONE;

    // Erase this page if it hasn't been erased
    if (!sectorErased) {
        // tr_debug("Erasing sector of size %d at address 0x%08x", get_sector_size(addr), addr);
        err = erase(addr, get_sector_size(addr));
        if (0 != err) {
            tr_error("Flash erase failed: %d", err);
            return err;
        }
        sectorErased = true;
    }

#if MBED_CONF_MBED_TRACE_ENABLE
    //if (pagesFlashed == 0) {
    //  tr_debug("%01x %01x %01x %01x %01x %01x %01x %01x", writePageBuffer[0], writePageBuffer[1], writePageBuffer[2],
    //           writePageBuffer[3], writePageBuffer[4], writePageBuffer[5], writePageBuffer[6], writePageBuffer[7]);
    //}
#endif

    // Program page
    err = program(writePageBuffer, addr, pageSize);
    if (0 != err) {
        tr_error("Flash program failed: %d (for %d bytes)", err, pageSize);
        return err;
    }
    //tr_debug("Program %d bytes at address 0x%08x", actual, addr);

    // check that was written is correct
    memset(readPageBuffer, 0, sizeof(char) * pageSize);
    err = read(readPageBuffer, addr, pageSize);
    if (0 != err) {
        tr_error("Flash read failed: %d", err);
        return err;
    }
    if (memcmp(writePageBuffer, readPageBuffer, pageSize) != 0) {
        tr_error("Write and read differ");
        return UC_ERR_WRITE_FAILED;
    }

    // update address and next sector
    pagesFlashed++;
    addr += pageSize;
    if (addr >= nextSectorAddress) {
        nextSectorAddress = addr + get_sector_size(addr);
        sectorErased = false;
    }

    return err;
}

uint32_t FlashUpdater::alignAddressToSector(uint32_t address, bool roundDown)
{
    // default to returning the beginning of the flash
    uint32_t sectorAlignedAddress = get_flash_start();
    uint32_t flashEndAddress = sectorAlignedAddress + get_flash_size();

    // addresses out of bounds are pinned to the flash boundaries
    if (address >= flashEndAddress) {
        sectorAlignedAddress = flashEndAddress;
    } else if (address > sectorAlignedAddress) {
        // for addresses within bounds step through the sector map
        uint32_t sectorSize = 0;

        // add sectors from start of flash until we exceed the required address
        // we cannot assume uniform sector size as in some mcu sectors have
        // drastically different sizes
        while (sectorAlignedAddress < address) {
            sectorSize = get_sector_size(sectorAlignedAddress);
            sectorAlignedAddress += sectorSize;
        }

        // if round down to nearest sector, remove the last sector from address
        // if not already aligned
        if (roundDown && (sectorAlignedAddress != address)) {
            sectorAlignedAddress -= sectorSize;
        }
    }

    return sectorAlignedAddress;
}

} // namespace update_client


