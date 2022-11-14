#pragma once

#include "mbed.h"
#include "USBSerial.h"
#include <chrono>

namespace update_client {

#if (USE_USB_SERIAL_UC == 1)

class USBSerialUC {

public:
    // constructor
    USBSerialUC();

    // method called for creating the CandidateApplications instance

    // methods for starting and stopping the updater
    void start();
    void stop();

private:
    // private method
    void downloadFirmware();

    // data members
    USBSerial _usbSerial;
    Thread _downloaderThread;
    enum {
        STOP_EVENT_FLAG = 1
    };
    EventFlags _stopEvent;
    static constexpr std::chrono::milliseconds kWaitTimeBetweenCheck = 5000ms;
};

#endif // USE_USB_SERIAL_UC

} // namespace

