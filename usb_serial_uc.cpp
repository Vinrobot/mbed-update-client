#include "usb_serial_uc.hpp"

#include "mbed_trace.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "USBSerialUC"
#endif // MBED_CONF_MBED_TRACE_ENABLE

#include "candidate_applications.hpp"
#include "flash_updater.hpp"
#include "uc_error_codes.hpp"

namespace update_client {

#if (USE_USB_SERIAL_UC == 1)

USBSerialUC::USBSerialUC() :
    _usbSerial(false),
    _downloaderThread(osPriorityNormal, OS_STACK_SIZE, nullptr, "DownloaderThread")
{

}

void USBSerialUC::start()
{
    _downloaderThread.start(callback(this, &USBSerialUC::downloadFirmware));
}

void USBSerialUC::stop()
{
    _stopEvent.set(STOP_EVENT_FLAG);
    _downloaderThread.join();
}

void USBSerialUC::downloadFirmware()
{
    while (true) {
        _usbSerial.connect();
        // we would use wait_ready() with a timeout here, but it is not possible
        // and since we want to make sure to be able to stop the thread

        // so check repeatidly if we are connected
        tr_debug("Waiting for connection");
        std::chrono::milliseconds sleep_for_time = kWaitTimeBetweenCheck;
        ThisThread::sleep_for(sleep_for_time);

        if (_usbSerial.connected()) {
            tr_debug("Updater connected");
            // flush the serial connection
            _usbSerial.sync();

            // initialize internal Flash
            FlashUpdater flashUpdater;
            int err = flashUpdater.init();
            if (0 != err) {
                tr_error("Init flash failed: %d", err);
                return;
            }
            const uint32_t pageSize = flashUpdater.get_page_size();

            std::unique_ptr<char> writePageBuffer = std::unique_ptr<char>(new char[pageSize]);
            std::unique_ptr<char> readPageBuffer = std::unique_ptr<char>(new char[pageSize]);

            // recompute the header size (accounting for alignment)
            const uint32_t headerSize = APPLICATION_ADDR - HEADER_ADDR;
            tr_debug(" Application header size is %" PRIu32 "", headerSize);

            // create the CandidateApplications instance for receiving the update
            std::unique_ptr<CandidateApplications> candidateApplications = std::unique_ptr<CandidateApplications>(
                createCandidateApplications(flashUpdater,
                                            MBED_CONF_UPDATE_CLIENT_STORAGE_ADDRESS,
                                            MBED_CONF_UPDATE_CLIENT_STORAGE_SIZE,
                                            headerSize,
                                            MBED_CONF_UPDATE_CLIENT_STORAGE_LOCATIONS));

            // get the slot index to be used for storing the candidate application
            tr_debug("Getting slot index...");
            uint32_t slotIndex = candidateApplications.get()->getSlotForCandidate();
    
            tr_debug("Reading application info for slot %" PRIu32 "", slotIndex);
            candidateApplications.get()->getMbedApplication(slotIndex).logApplicationInfo();
            
            uint32_t candidateApplicationAddress = 0;            
            uint32_t slotSize = 0;
            int32_t result = candidateApplications.get()->getCandidateAddress(slotIndex, 
                                                                              candidateApplicationAddress, 
                                                                              slotSize);
            if (result != UC_ERR_NONE) {
                tr_error("getCandidateAddress failed: %" PRIi32 "", result);
                return;
            }
            uint32_t addr = candidateApplicationAddress;
            uint32_t sectorSize = flashUpdater.get_sector_size(addr);
            tr_debug("Using slot %" PRIu32 " and starting to write at address 0x%08" PRIx32 " with sector size %" PRIu32 " (aligned %" PRIu32 ")", 
                     slotIndex, addr, sectorSize, addr % sectorSize);

            uint32_t nextSector = addr + sectorSize;
            bool sectorErased = false;
            size_t pagesFlashed = 0;

            tr_debug("Please send the update file...");

            uint32_t nbrOfBytes = 0;
            while (_usbSerial.connected()) {
                // receive data for this page
                memset(writePageBuffer.get(), 0, sizeof(char) * pageSize);
                for (uint32_t i = 0; i < pageSize; i++) {
                    writePageBuffer.get()[i] = _usbSerial.getc();
                }

                // write the page to the flash
                flashUpdater.writePage(pageSize, writePageBuffer.get(), readPageBuffer.get(),
                                       addr, sectorErased, pagesFlashed, nextSector);

                // update progress
                nbrOfBytes += pageSize;
                printf("Received %05" PRIu32 " bytes\r", nbrOfBytes);
            }

            // compare the active application with the downloaded one
            uint32_t activeApplicationHeaderAddress = MBED_ROM_START + MBED_CONF_TARGET_HEADER_OFFSET;
            uint32_t activeApplicationAddress = activeApplicationHeaderAddress + headerSize;
            update_client::MbedApplication activeApplication(flashUpdater, 
                                                             activeApplicationHeaderAddress, 
                                                             activeApplicationAddress);

            update_client::MbedApplication candidateApplication(flashUpdater, 
                                                                candidateApplicationAddress, 
                                                                candidateApplicationAddress + headerSize);
            activeApplication.compareTo(candidateApplication);

            writePageBuffer = NULL;
            readPageBuffer = NULL;

            flashUpdater.deinit();

            tr_debug("Nbr of bytes received %" PRIu32 "", nbrOfBytes);
        }

        // check whether the thread has been stopped
        if (_stopEvent.wait_all_for(STOP_EVENT_FLAG, std::chrono::milliseconds::zero()) == STOP_EVENT_FLAG) {
            // exit the loop and the thread
            tr_debug("Exiting downloadFirmware");
            break;
        }
    }

}

#endif // USE_USB_SERIAL_UC

} // namespace update_client
