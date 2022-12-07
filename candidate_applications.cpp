#include "candidate_applications.hpp"
#include "flash_updater.hpp"
#include "uc_error_codes.hpp"
#include <cstdint>

#include "mbed_trace.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "CandidateApplications"
#endif // MBED_CONF_MBED_TRACE_ENABLE

MBED_WEAK update_client::CandidateApplications* createCandidateApplications(update_client::FlashUpdater &flashUpdater, 
                                                             uint32_t storageAddress, 
                                                             uint32_t storageSize, 
                                                             uint32_t headerSize, 
                                                             uint32_t nbrOfSlots)
{
    return new update_client::CandidateApplications(flashUpdater, storageAddress, storageSize, headerSize, nbrOfSlots);
}
             
namespace update_client {
                                                
CandidateApplications::CandidateApplications(FlashUpdater &flashUpdater,
                                             uint32_t storageAddress,
                                             uint32_t storageSize,
                                             uint32_t headerSize,
                                             uint32_t nbrOfSlots) :
    _flashUpdater(flashUpdater),
    _storageAddress(storageAddress),
    _storageSize(storageSize),
    _nbrOfSlots(nbrOfSlots)
{
    // the number of slots must be equal or smaller than MBED_CONF_UPDATE_CLIENT_STORAGE_LOCATIONS
    if (nbrOfSlots <= MBED_CONF_UPDATE_CLIENT_STORAGE_LOCATIONS) {
        for (uint32_t slotIndex = 0; slotIndex < nbrOfSlots; slotIndex++) {
            uint32_t candidateAddress = 0;
            uint32_t slotSize = 0;
            int32_t result = getCandidateAddress(slotIndex, candidateAddress, slotSize);
            if (result != UC_ERR_NONE) {
                tr_error(" Application at slot %" PRIu32 " is not valid: %" PRIi32 "", slotIndex, result);
                continue;
            }

            tr_debug(" Slot %" PRIu32 ": application header address: 0x%08" PRIx32 " application address 0x%08" PRIx32 " (slot size %" PRIu32 ")",
                     slotIndex, candidateAddress, candidateAddress + headerSize, slotSize);
            _candidateApplicationArray[slotIndex] = new update_client::MbedApplication(_flashUpdater,
                                                                                       candidateAddress,
                                                                                       candidateAddress + headerSize);
        }
    }
}

CandidateApplications::~CandidateApplications()
{
    for (uint32_t slotIndex = 0; slotIndex < _nbrOfSlots; slotIndex++) {
        delete _candidateApplicationArray[slotIndex];
        _candidateApplicationArray[slotIndex] = NULL;
    }
}

uint32_t CandidateApplications::getSlotForCandidate()
{
    // default implementation, always returns 0
    return 0;
}

uint32_t CandidateApplications::getNbrOfSlots() const 
{
    return _nbrOfSlots;
}

MbedApplication &CandidateApplications::getMbedApplication(uint32_t slotIndex)
{
    return *_candidateApplicationArray[slotIndex];
}


int32_t CandidateApplications::getCandidateAddress(uint32_t slotIndex,
                                                   uint32_t &candidateAddress,
                                                   uint32_t &slotSize) const
{
    // find the start address of the whole storage area. It needs to be aligned to
    // sector boundary and we cannot go outside user defined storage area, hence
    // rounding up to sector boundary
    uint32_t storageStartAddr = _flashUpdater.alignAddressToSector(_storageAddress, false);
    
    // find the end address of the whole storage area. It needs to be aligned to
    // sector boundary and we cannot go outside user defined storage area, hence
    // rounding down to sector boundary
    uint32_t storageEndAddr = _flashUpdater.alignAddressToSector(_storageAddress + _storageSize, true);
    
    // find the maximum size each slot can have given the start and end, without
    // considering the alignment of individual slots
    uint32_t maxSlotSize = (storageEndAddr - storageStartAddr) / _nbrOfSlots;
    
    // find the start address of slot. It needs to align to sector boundary. We
    // choose here to round down at each slot boundary
    uint32_t slotStartAddr = _flashUpdater.alignAddressToSector(storageStartAddr + slotIndex * maxSlotSize, true);
    
    // find the end address of the slot, rounding down to sector boundary same as
    // the slot start address so that we make sure two slot don't overlap
    uint32_t slotEndAddr = _flashUpdater.alignAddressToSector(slotStartAddr + maxSlotSize, true);
    
    candidateAddress = slotStartAddr;
    slotSize = slotEndAddr - slotStartAddr;

    return UC_ERR_NONE;
}

void CandidateApplications::logCandidateAddress(uint32_t slotIndex) const
{
    tr_debug(" Slot %" PRIu32 ": Storage address: 0x%08" PRIx32 " Storage size: %" PRIu32 "", slotIndex, _storageAddress, _storageSize);

    // find the start address of the whole storage area. It needs to be aligned to
    // sector boundary and we cannot go outside user defined storage area, hence
    // rounding up to sector boundary
    uint32_t storageStartAddr = _flashUpdater.alignAddressToSector(_storageAddress, false);
    tr_debug(" Storage start address (slot %" PRIu32 "): 0x%08" PRIx32 "", slotIndex, storageStartAddr);

    // find the end address of the whole storage area. It needs to be aligned to
    // sector boundary and we cannot go outside user defined storage area, hence
    // rounding down to sector boundary
    uint32_t storageEndAddr = _flashUpdater.alignAddressToSector(_storageAddress + _storageSize, true);
    tr_debug(" Storage end address (slot %" PRIu32 "): 0x%08" PRIx32 "", slotIndex, storageEndAddr);

    // find the maximum size each slot can have given the start and end, without
    // considering the alignment of individual slots
    uint32_t maxSlotSize = (storageEndAddr - storageStartAddr) / _nbrOfSlots;
    tr_debug(" maxSlotSize (slot %" PRIu32 "): %" PRIu32 "", slotIndex, maxSlotSize);

    // find the start address of slot. It needs to align to sector boundary. We
    // choose here to round down at each slot boundary
    uint32_t slotStartAddr = _flashUpdater.alignAddressToSector(storageStartAddr + slotIndex * maxSlotSize, true);
    tr_debug(" Slot start address (slot %" PRIu32 "): 0x%08" PRIx32 "", slotIndex, slotStartAddr);

    // find the end address of the slot, rounding down to sector boundary same as
    // the slot start address so that we make sure two slot don't overlap
    uint32_t slotEndAddr = _flashUpdater.alignAddressToSector(slotStartAddr + maxSlotSize, true);
    tr_debug(" Slot end address (slot %" PRIu32 "): 0x%08" PRIx32 "", slotIndex, slotEndAddr);
}
    
bool CandidateApplications::hasValidNewerApplication(MbedApplication &activeApplication,
                                                     uint32_t &newestSlotIndex) const
{
    tr_debug(" Checking for newer applications on %" PRIu32 " slots", _nbrOfSlots);
    newestSlotIndex = _nbrOfSlots;
    for (uint32_t slotIndex = 0; slotIndex < _nbrOfSlots; slotIndex++) {
        // Only hash check firmwares with higher version number than the
        // active image and with a different hash. This prevents rollbacks
        // and hash checks of old images. If the active image is not valid,
        // bestStoredFirmwareImageDetails.version equals 0
        tr_debug(" Checking application at slot %" PRIu32 "", slotIndex);
        MbedApplication &newestApplication = newestSlotIndex == _nbrOfSlots ?
                                             activeApplication : *_candidateApplicationArray[newestSlotIndex];

        if (_candidateApplicationArray[slotIndex]->isNewerThan(newestApplication)) {
#if MBED_CONF_MBED_TRACE_ENABLE
            if (newestSlotIndex == _nbrOfSlots) {
                tr_debug(" Candidate application at slot %" PRIu32 " is newer than the active one", slotIndex);
            } else {
                tr_debug(" Candidate application at slot %" PRIu32 " is newer than application at slot %" PRIu32 "",
                         slotIndex, newestSlotIndex);
            }
#endif
            int32_t result = _candidateApplicationArray[slotIndex]->checkApplication();
            if (result != UC_ERR_NONE) {
                tr_error(" Candidate application at slot %" PRIu32 " is not valid: %" PRIi32 "", slotIndex, result);
                continue;
            }
            tr_debug(" Candidate application at slot %" PRIu32 " is valid", slotIndex);

            // update the newest slot index
            newestSlotIndex = slotIndex;
        }
    }
    return newestSlotIndex != _nbrOfSlots;
}

#if defined(POST_APPLICATION_ADDR)
int32_t CandidateApplications::installApplication(uint32_t slotIndex, uint32_t destHeaderAddress)
{
    tr_debug(" Installing candidate application at slot %d as active application", slotIndex);
    const uint32_t pageSize = _flashUpdater.get_page_size();
    tr_debug("Flash page size is %d", pageSize);

    std::unique_ptr<char> writePageBuffer = std::unique_ptr<char>(new char[pageSize]);
    std::unique_ptr<char> readPageBuffer = std::unique_ptr<char>(new char[pageSize]);

    uint32_t destAddr = destHeaderAddress;
    uint32_t sourceAddr = 0;
    uint32_t slotSize = 0;
    int32_t result = getCandidateAddress(slotIndex, sourceAddr, slotSize);
    if (result != UC_ERR_NONE) {
        tr_error("Cannot get address of candidate application at slot %d", slotIndex);
        return result;
    }

    const uint32_t destSectorSize = _flashUpdater.get_sector_size(destAddr);
    uint32_t nextDestSectorAddress = destAddr + destSectorSize;
    bool destSectorErased = false;
    size_t destPagesFlashed = 0;

    // add the header size to the firmware size
    const uint32_t headerSize = POST_APPLICATION_ADDR - HEADER_ADDR;
    tr_debug(" Header size is %d", headerSize);
    const uint64_t copySize = _candidateApplicationArray[slotIndex]->getFirmwareSize() + headerSize;

    uint32_t nbrOfBytes = 0;
    tr_debug(" Starting to copy application from address 0x%08x to address 0x%08x", sourceAddr, destAddr);

    while (nbrOfBytes < copySize) {
        // read the page from the candidate application
        result = _flashUpdater.readPage(pageSize, writePageBuffer.get(), sourceAddr);
        if (result != UC_ERR_NONE) {
            tr_error("Cannot read candidate application at slot %d (address 0x%08x)", slotIndex, sourceAddr);
            return result;
        }

        // write the page to the flash active application address
        // destAddr and beyond are modified in the writePage method
        result = _flashUpdater.writePage(pageSize, writePageBuffer.get(), readPageBuffer.get(),
                                        destAddr, destSectorErased, destPagesFlashed, nextDestSectorAddress);
        if (result != UC_ERR_NONE) {
            tr_error("Cannot write candidate application at slot %d (address 0x%08x)", slotIndex, destAddr);
            return result;
        }
        
        // update progress
        nbrOfBytes += pageSize;
#if MBED_CONF_MBED_TRACE_ENABLE
        // tr_debug("Copied %05d bytes", nbrOfBytes);
#endif
    }
    tr_debug(" Copied %d bytes", nbrOfBytes);
    writePageBuffer = NULL;
    readPageBuffer = NULL;

    return UC_ERR_NONE;
}
#endif

} // namespace update_client
