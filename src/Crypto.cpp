#include "Crypto.h"

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>        // CRYPTO_memcmp

#include <cstdlib>
#include <vector>

namespace Crypto {

namespace {

const int         PBKDF2_ITERATIONS = 100000;   // 迭代次数（越大越安全、越慢）
const int         SALT_LEN          = 16;       // 16 字节随机盐
const int         HASH_LEN          = 32;       // SHA-256 输出 32 字节
const char*       SCHEME            = "pbkdf2_sha256";

std::string toHex(const unsigned char* data, std::size_t len)
{
    static const char hexchars[] = "0123456789abcdef";

    std::string out;
    out.reserve(len * 2);

    for (std::size_t i = 0; i < len; i++)
    {
        out.push_back(hexchars[data[i] >> 4]);
        out.push_back(hexchars[data[i] & 0x0F]);
    }
    return out;
}

std::vector<unsigned char> fromHex(const std::string& hex)
{
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);

    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        int hi = std::isdigit((unsigned char)hex[i]) ? hex[i] - '0' : (hex[i] | 32) - 'a' + 10;
        int lo = std::isdigit((unsigned char)hex[i+1]) ? hex[i+1] - '0' : (hex[i+1] | 32) - 'a' + 10;
        out.push_back((unsigned char)((hi << 4) | lo));
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> parts;
    std::string cur;

    for (char c : s)
    {
        if (c == delim)
        {
            parts.push_back(cur);
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);

    return parts;
}

}   // namespace

std::string sha256Hex(const std::string& input)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];

    SHA256(reinterpret_cast<const unsigned char*>(input.data()),
           input.size(),
           digest);

    return toHex(digest, SHA256_DIGEST_LENGTH);
}

std::string hashPassword(const std::string& password)
{
    // 1) 每个用户一个独立的随机盐
    unsigned char salt[SALT_LEN];

    if (RAND_bytes(salt, SALT_LEN) != 1)
    {
        return "";                      // 随机数失败（极端情况）
    }

    // 2) PBKDF2：把哈希迭代很多次，拖慢暴力破解
    unsigned char hash[HASH_LEN];

    PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                      salt, SALT_LEN,
                      PBKDF2_ITERATIONS,
                      EVP_sha256(),
                      HASH_LEN, hash);

    // 3) 拼成自描述的存储串（算法/参数都带上，将来可平滑升级）
    return std::string(SCHEME) + "$" +
           std::to_string(PBKDF2_ITERATIONS) + "$" +
           toHex(salt, SALT_LEN) + "$" +
           toHex(hash, HASH_LEN);
}

bool verifyPassword(const std::string& password, const std::string& stored)
{
    // 解析存储串
    std::vector<std::string> parts = split(stored, '$');

    if (parts.size() != 4 || parts[0] != SCHEME)
    {
        return false;                   // 不是我们的格式
    }

    int iterations = std::atoi(parts[1].c_str());
    if (iterations <= 0)
    {
        return false;
    }

    std::vector<unsigned char> salt     = fromHex(parts[2]);
    std::vector<unsigned char> expected = fromHex(parts[3]);

    if (salt.empty() || expected.empty())
    {
        return false;
    }

    // 用同样的参数重算
    std::vector<unsigned char> computed(expected.size());

    PKCS5_PBKDF2_HMAC(password.c_str(), (int)password.size(),
                      salt.data(), (int)salt.size(),
                      iterations,
                      EVP_sha256(),
                      (int)computed.size(), computed.data());

    // ★ 常量时间比对：普通 == 会在第一个不同的字节处提前返回，
    //   攻击者能通过测量响应时间逐字节猜出哈希（时序攻击）
    return CRYPTO_memcmp(computed.data(), expected.data(), expected.size()) == 0;
}

}
