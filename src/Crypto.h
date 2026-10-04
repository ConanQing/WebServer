#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

namespace Crypto {

// 算字符串的 SHA-256，返回 64 字符的十六进制小写串
std::string sha256Hex(const std::string& input);

}

#endif
