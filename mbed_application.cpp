#include "mbed_application.hpp"
#include "uc_error_codes.hpp"

#include "mbed_trace.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "MbedApplication"
#endif // MBED_CONF_MBED_TRACE_ENABLE

//#include "bootloader_mbedtls_user_config.h"

#include "mbedtls/sha256.h"

namespace update_client {

MbedApplication::MbedApplication(FlashUpdater &flashUpdater,
                                 uint32_t applicationHeaderAddress,
                                 uint32_t applicationAddress) :
    _flashUpdater(flashUpdater),
    _applicationHeaderAddress(applicationHeaderAddress),
    _applicationAddress(applicationAddress)
{
    memset(_buffer, 0, sizeof(_buffer));
    memset((void *) &_applicationHeader, 0, sizeof(_applicationHeader));
    _applicationHeader.initialized = false;
    _applicationHeader.state = NOT_CHECKED;
}

bool MbedApplication::isValid()
{
    if (! _applicationHeader.initialized) {
        int32_t result = readApplicationHeader();
        if (result != UC_ERR_NONE) {
            _applicationHeader.state = NOT_VALID;
        }
    }
    if (_applicationHeader.state == NOT_CHECKED) {
        int32_t result = checkApplication();
        if (result != UC_ERR_NONE) {
            _applicationHeader.state = NOT_VALID;
        }
    }

    return _applicationHeader.state != NOT_VALID;
}

uint64_t MbedApplication::getFirmwareVersion()
{
    if (! _applicationHeader.initialized) {
        int32_t result = readApplicationHeader();
        if (result != UC_ERR_NONE) {
            tr_error(" Invalid application header: %d", result);
            _applicationHeader.state = NOT_VALID;
            return 0;
        }
    }

    return _applicationHeader.firmwareVersion;
}

uint64_t MbedApplication::getFirmwareSize()
{
    if (! _applicationHeader.initialized) {
        int32_t result = readApplicationHeader();
        if (result != UC_ERR_NONE) {
            tr_error(" Invalid application header: %d", result);
            _applicationHeader.state = NOT_VALID;
            return 0;
        }
    }

    return _applicationHeader.firmwareSize;
}

bool MbedApplication::isNewerThan(MbedApplication &otherApplication)
{
    // read application header if required
    if (! _applicationHeader.initialized) {
        readApplicationHeader();
    }
    if (! otherApplication._applicationHeader.initialized) {
        otherApplication.readApplicationHeader();
    }

    // if this application is not valid or empty, it cannot be newer
    if (_applicationHeader.headerVersion < kHeaderVersionV2 ||
            _applicationHeader.firmwareSize == 0 ||
            _applicationHeader.state == NOT_VALID) {
        return false;
    }
    // if the other application is not valid or empty, this one is newer
    if (otherApplication._applicationHeader.headerVersion < kHeaderVersionV2 ||
            otherApplication._applicationHeader.firmwareSize == 0 ||
            otherApplication._applicationHeader.state == NOT_VALID) {
        return true;
    }

    // both applications are valid and not empty
    return otherApplication._applicationHeader.firmwareVersion < _applicationHeader.firmwareVersion;
}

int32_t MbedApplication::checkApplication()
{
    // read the header
    int32_t result = readApplicationHeader();
    if (result != UC_ERR_NONE) {
        tr_error(" Invalid application header: %d", result);
        _applicationHeader.state = NOT_VALID;
        return result;
    }
    tr_debug(" Application size is %lld", _applicationHeader.firmwareSize);

    // at this stage, the header is valid
    // calculate hash if slot is not empty
    if (_applicationHeader.firmwareSize > 0) {
        // initialize hashing facility
        mbedtls_sha256_context mbedtls_ctx;
        mbedtls_sha256_init(&mbedtls_ctx);
        mbedtls_sha256_starts(&mbedtls_ctx, 0);

        uint8_t SHA[kSizeOfSHA256] = { 0 };
        uint32_t remaining = _applicationHeader.firmwareSize;

        // read full image
        tr_debug(" Calculating hash (start address 0x%08x, size %lld)",
                 _applicationAddress, _applicationHeader.firmwareSize);
        while (remaining > 0) {
            // read full buffer or what is remaining
            uint32_t readSize = (remaining > kBufferSize) ? kBufferSize : remaining;

            // read buffer using FlashIAP API for portability */
            int err = _flashUpdater.read(_buffer,
                                         _applicationAddress + (_applicationHeader.firmwareSize - remaining),
                                         readSize);
            if (err != 0) {
                tr_error(" Error while reading flash %d", err);
                result = UC_ERR_READING_FLASH;
                break;
            }

            // update hash
            mbedtls_sha256_update(&mbedtls_ctx, _buffer, readSize);

            // update remaining bytes
            remaining -= readSize;
        }

        // finalize hash
        mbedtls_sha256_finish(&mbedtls_ctx, SHA);
        mbedtls_sha256_free(&mbedtls_ctx);

        // compare calculated hash with hash from header
        int diff = memcmp(_applicationHeader.hash, SHA, kSizeOfSHA256);

        if (diff == 0) {
            result = UC_ERR_NONE;
        } else {
            result = UC_ERR_HASH_INVALID;
        }
    } else {
        // header is valid but application size is 0
        result = UC_ERR_FIRMWARE_EMPTY;
    }
    if (result == UC_ERR_NONE) {
        _applicationHeader.state = VALID;
    } else {
        _applicationHeader.state = NOT_VALID;
    }
    return result;
}

void MbedApplication::logApplicationInfo() const
{  
    if (! _applicationHeader.initialized) {
        tr_debug("Application not initialized");
    }
    else {
        tr_debug(" Magic %d, Version %d", _applicationHeader.magic, _applicationHeader.headerVersion);
    }
}

void MbedApplication::compareTo(MbedApplication &otherApplication)
{
    tr_debug(" Comparing applications at address 0x%08x and 0x%08x",
             _applicationAddress, otherApplication._applicationAddress); 

    tr_debug(" Checking application at address 0x%08x", _applicationAddress);
    int32_t result = checkApplication();
    if (result != UC_ERR_NONE) {
        tr_error(" Application is not valid");
        return;
    }
    tr_debug(" Checking application at address 0x%08x", otherApplication._applicationAddress);
    result = otherApplication.checkApplication();
    if (result != UC_ERR_NONE) {
        tr_error(" Application is not valid");
        return;
    }
    tr_debug(" Both applications are valid");

    if (_applicationHeader.magic != otherApplication._applicationHeader.magic) {
        tr_debug("Magic numbers differ");
    }
    if (_applicationHeader.headerVersion != otherApplication._applicationHeader.headerVersion) {
        tr_debug("Header versions differ");
    }
    if (_applicationHeader.firmwareSize != otherApplication._applicationHeader.firmwareSize) {
        tr_debug("Firmware sizes differ");
    }
    if (_applicationHeader.firmwareVersion != otherApplication._applicationHeader.firmwareVersion) {
        tr_debug("Firmware versions differ");
    }
    if (memcmp(_applicationHeader.hash,
               otherApplication._applicationHeader.hash,
               sizeof(_applicationHeader.hash)) != 0) {
        tr_debug("Hash differ");
    }

    if (_applicationHeader.firmwareSize == otherApplication._applicationHeader.firmwareSize) {
        tr_debug(" Comparing application binaries");
        const uint32_t pageSize = _flashUpdater.get_page_size();
        tr_debug("Flash page size is %d", pageSize);

        std::unique_ptr<char> readPageBuffer1 = std::unique_ptr<char>(new char[pageSize]);
        std::unique_ptr<char> readPageBuffer2 = std::unique_ptr<char>(new char[pageSize]);
        uint32_t address1 = _applicationAddress;
        uint32_t address2 = otherApplication._applicationAddress;
        uint32_t nbrOfBytes = 0;
        bool binariesMatch = true;
        while (nbrOfBytes < _applicationHeader.firmwareSize) {
            result = _flashUpdater.readPage(pageSize, readPageBuffer1.get(), address1);
            if (result != UC_ERR_NONE) {
                tr_error("Cannot read application 1 (address 0x%08x)", address1);
                binariesMatch = false;
                break;
            }
            result = _flashUpdater.readPage(pageSize, readPageBuffer2.get(), address2);
            if (result != UC_ERR_NONE) {
                tr_error("Cannot read application 2 (address 0x%08x)", address2);
                binariesMatch = false;
                break;
            }

            if (memcmp(readPageBuffer1.get(), readPageBuffer2.get(), pageSize) != 0) {
                tr_error("Applications differ at byte %d (address1 0x%08x - address2 0x%08x)",
                         nbrOfBytes, address1, address2);
                binariesMatch = false;
                break;
            }
            nbrOfBytes += pageSize;
        }

        if (binariesMatch) {
            tr_debug("Application binaries are identical");
        }
    }
}

int32_t MbedApplication::readApplicationHeader()
{
    // default return code
    int32_t result = UC_ERR_INVALID_HEADER;

    // read magic number and version
    uint8_t version_buffer[8] = { 0 };
    int err = _flashUpdater.read(version_buffer, _applicationHeaderAddress, 8);
    if (0 == err) {
        // read out header magic
        _applicationHeader.magic = parseUint32(&version_buffer[0]);
        // read out header magic
        _applicationHeader.headerVersion = parseUint32(&version_buffer[4]);

        // choose version to decode
        switch (_applicationHeader.headerVersion) {
            case kHeaderVersionV2: {
                result = UC_ERR_NONE;
                // Check the header magic
                if (_applicationHeader.magic == KheaderMagicV2) {
                    uint8_t read_buffer[kHeaderSizeV2] = { 0 };
                    // read the rest of header (V2)
                    err = _flashUpdater.read(read_buffer, _applicationHeaderAddress, kHeaderSizeV2);
                    if (err == 0) {
                        // parse the header
                        result = parseInternalHeaderV2(read_buffer);
                        if (result != UC_ERR_NONE) {
                            tr_error(" Failed to parse header: %d", result);
                        }
                    } else {
                        tr_error("Flash read failed: %d", err);
                        result = UC_ERR_READING_FLASH;
                    }
                } else {
                    tr_error(" Invalid magic number");
                    result = UC_ERR_INVALID_HEADER;
                }
            }
            break;

        // Other firmware header versions can be supported here
    default:
        break;
    }
} else
{
    tr_error("Flash read failed: %d", err);
    result = UC_ERR_READING_FLASH;
}

_applicationHeader.initialized = true;
if (result == UC_ERR_NONE)
{
    _applicationHeader.state = VALID;
} else
{
    _applicationHeader.state = NOT_VALID;
}

return result;
}

int32_t MbedApplication::parseInternalHeaderV2(const uint8_t *pBuffer)
{
    // we expect pBuffer to contain the entire header (version 2)
    int32_t result = UC_ERR_INVALID_HEADER;

    if (pBuffer != NULL) {
        // calculate CRC
        uint32_t calculatedChecksum = crc32(pBuffer, kHeaderCrcOffsetV2);

        // read out CRC
        uint32_t temp32 = parseUint32(&pBuffer[kHeaderCrcOffsetV2]);

        if (temp32 == calculatedChecksum) {
            // parse content
            _applicationHeader.firmwareVersion = parseUint64(&pBuffer[kFirmwareVersionOffsetV2]);
            _applicationHeader.firmwareSize = parseUint64(&pBuffer[kFirmwareSizeOffsetV2]);

            tr_debug(" headerVersion %d, firmwareVersion %lld, firmwareSize %lld",
                     _applicationHeader.headerVersion, _applicationHeader.firmwareVersion,
                     _applicationHeader.firmwareSize);

            memcpy(_applicationHeader.hash, &pBuffer[kHashOffsetV2], SHA256_SIZE);
            memcpy(_applicationHeader.campaign, &pBuffer[kCampaingOffetV2], GUID_SIZE);

            // set result
            result = UC_ERR_NONE;
        } else {
            result = UC_ERR_INVALID_CHECKSUM;
        }
    }

    return result;
}

uint32_t MbedApplication::parseUint32(const uint8_t *pBuffer)
{
    uint32_t result = 0;
    if (pBuffer) {
        result = pBuffer[0];
        result = (result << 8) | pBuffer[1];
        result = (result << 8) | pBuffer[2];
        result = (result << 8) | pBuffer[3];
    }

    return result;
}

uint64_t MbedApplication::parseUint64(const uint8_t *pBuffer)
{
    uint64_t result = 0;
    if (pBuffer) {
        result = pBuffer[0];
        result = (result << 8) | pBuffer[1];
        result = (result << 8) | pBuffer[2];
        result = (result << 8) | pBuffer[3];
        result = (result << 8) | pBuffer[4];
        result = (result << 8) | pBuffer[5];
        result = (result << 8) | pBuffer[6];
        result = (result << 8) | pBuffer[7];
    }

    return result;
}

uint32_t MbedApplication::crc32(const uint8_t *pBuffer, uint32_t length)
{
    const uint8_t *pCurrent = pBuffer;
    uint32_t crc = 0xFFFFFFFF;

    while (length--) {
        crc ^= *pCurrent;
        pCurrent++;

        for (uint32_t counter = 0; counter < 8; counter++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return (crc ^ 0xFFFFFFFF);
}

} // namesapce
