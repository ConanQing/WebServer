#include "Crypto.h"

#include <openssl/sha.h>

namespace Crypto {

std::string sha256Hex(const std::string& input)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];     // 32 字节

    SHA256(reinterpret_cast<const unsigned char*>(input.data()),
           input.size(),
           digest);

    static const char hexchars[] = "0123456789abcdef";

    std::string out;
    out.reserve(SHA256_DIGEST_LENGTH * 2);          // 64 字符

    for (unsigned char c : digest)
    {
        out.push_back(hexchars[c >> 4]);            // 高 4 位
        out.push_back(hexchars[c & 0x0F]);          // 低 4 位
    }

    return out;
}

}
