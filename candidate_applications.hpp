#pragma once

#include "mbed.h"

#include "mbed_application.hpp"
#include "flash_updater.hpp"

namespace update_client {

class CandidateApplications {
public:
    CandidateApplications(FlashUpdater &flashUpdater, uint32_t storageAddress, uint32_t storageSize, 
                          uint32_t headerSize, uint32_t nbrOfSlots);
    virtual ~CandidateApplications();

    // methods that can be overriden 
    virtual uint32_t getSlotForCandidate();

    // public methods
    uint32_t getNbrOfSlots() const;
    MbedApplication &getMbedApplication(uint32_t slotIndex);
    int32_t getCandidateAddress(uint32_t slotIndex, uint32_t &applicationAddress, uint32_t &slotSize) const;
    void logCandidateAddress(uint32_t slotIndex) const;
    bool hasValidNewerApplication(MbedApplication &activeApplication, uint32_t &newestSlotIndex) const;
    // the installApplication method is used by the bootloader application
    // (for which the POST_APPLICATION_ADDR symbol is defined)
#if defined(POST_APPLICATION_ADDR)
    int32_t installApplication(uint32_t slotIndex, uint32_t destHeaderAddress);
#endif

private:
    // data members
    FlashUpdater &_flashUpdater;
    uint32_t _storageAddress;
    uint32_t _storageSize;
    uint32_t _nbrOfSlots;
    MbedApplication *_candidateApplicationArray[MBED_CONF_UPDATE_CLIENT_STORAGE_LOCATIONS];
};
                                                            
} // namespace update_client


update_client::CandidateApplications* createCandidateApplications(update_client::FlashUpdater &flashUpdater, 
                                                                  uint32_t storageAddress, 
                                                                  uint32_t storageSize, 
                                                                  uint32_t headerSize, 
                                                                  uint32_t nbrOfSlots);