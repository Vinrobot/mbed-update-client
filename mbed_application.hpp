#pragma once

#include "mbed.h"

#include "flash_updater.hpp"

namespace update_client {

class MbedApplication {
public:
    // constructor
    MbedApplication(FlashUpdater &flashUpdater, uint32_t applicationHeaderAddress, uint32_t applicationAddress);
    
    // public methods
    bool isValid();
    uint64_t getFirmwareVersion();
    uint64_t getFirmwareSize();
    bool isNewerThan(MbedApplication &otherApplication);
    int32_t checkApplication();
    void logApplicationInfo() const;
    void compareTo(MbedApplication &otherApplication);

private:
    // private methods
    int32_t readApplicationHeader();
    int32_t parseInternalHeaderV2(const uint8_t *pBuffer);

    static uint32_t parseUint32(const uint8_t *pBuffer);
    static uint64_t parseUint64(const uint8_t *pBuffer);
    static uint32_t crc32(const uint8_t *pBuffer, uint32_t length);

    // data members
    FlashUpdater &_flashUpdater;
    const uint32_t _applicationHeaderAddress;
    const uint32_t _applicationAddress;

    // application header
    // GUID type
    static constexpr uint8_t GUID_SIZE = (128 / 8);
    typedef uint8_t guid_t[GUID_SIZE];

    // SHA256 hash
    static constexpr uint8_t SHA256_SIZE = (256 / 8);
    typedef uint8_t hash_t[SHA256_SIZE];

    enum ApplicationState {
        NOT_CHECKED,
        VALID,
        NOT_VALID
    };
    struct ApplicationHeader {
        bool initialized;
        uint32_t magic;
        uint32_t headerVersion;
        uint64_t firmwareVersion;
        uint64_t firmwareSize;
        hash_t hash;
        guid_t campaign;
        uint32_t signatureSize;
        uint8_t signature[0];
        ApplicationState state;
    };
    ApplicationHeader _applicationHeader;

    // the size and offsets defined below do not correspond to the
    // application header defined above but rather to the definition in
    // the mbed_lib.json file
    // constants defining the header
    static constexpr uint32_t kHeaderVersionV2 = 2;
    static constexpr uint32_t KheaderMagicV2 = 0x5a51b3d4UL;
    static constexpr uint32_t kHeaderSizeV2 = 112;
    static constexpr uint32_t kFirmwareVersionOffsetV2 = 8;
    static constexpr uint32_t kFirmwareSizeOffsetV2 = 16;
    static constexpr uint32_t kHashOffsetV2 = 24;
    static constexpr uint32_t kCampaingOffetV2 = 88;
    static constexpr uint32_t kSignatureSizeOffsetV2 = 104;
    static constexpr uint32_t kHeaderCrcOffsetV2 = 108;

    // other constants
    static constexpr uint32_t kSizeOfSHA256 = (256 / 8);
    static constexpr uint32_t kBufferSize = 256;
    // buffer used in storage operations
    uint8_t _buffer[kBufferSize];
};

} // namespace update_client