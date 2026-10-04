#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

namespace Crypto {

// 算字符串的 SHA-256，返回 64 字符十六进制小写串
std::string sha256Hex(const std::string& input);

// ---------- 密码哈希（PBKDF2-HMAC-SHA256 + 随机盐）----------

// 生成密码的存储串，格式：
//   pbkdf2_sha256$<迭代次数>$<盐hex>$<哈希hex>
std::string hashPassword(const std::string& password);

// 校验密码是否匹配存储串（解析出算法/迭代/盐，重算后常量时间比对）
bool verifyPassword(const std::string& password, const std::string& stored);

}

#endif
