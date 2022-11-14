#pragma once

namespace update_client {

// return codes
enum UC_RETURN_CODES {
    UC_ERR_NONE = 0,
    UC_ERR_INVALID_HEADER = -1,
    UC_ERR_INVALID_CHECKSUM = -2,
    UC_ERR_READING_FLASH = -3,
    UC_ERR_HASH_INVALID = -4,
    UC_ERR_FIRMWARE_EMPTY = -5,
    UC_ERR_WRITE_FAILED = -6
};

} // namespace update_client