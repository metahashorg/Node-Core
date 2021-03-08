#ifndef METANET_META_COMMON_H
#define METANET_META_COMMON_H

#include <cstdint>

namespace metahash::network {

const uint32_t METAHASH_MAGIC_NUMBER = 0xabcd0001;

namespace statics {
    enum parse_state {
        SUCCESS,
        INCOMPLETE,
        WRONG_MAGIC_NUMBER,
        UNKNOWN_SENDER_METAHASH_ADDRESS,
        INVALID_SIGN,
    };
}

}
#endif //METANET_META_COMMON_H
