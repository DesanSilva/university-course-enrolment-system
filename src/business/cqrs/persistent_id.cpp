#include "nexusenroll/business/cqrs/persistent_id.hpp"

#include <array>
#include <random>

namespace nexusenroll::business::cqrs {

using namespace std;

string newPersistentId(const string& prefix) {
    static constexpr char hexadecimal[] = "0123456789abcdef";
    random_device source;
    array<unsigned char, 16> bytes{};
    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(source());
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);

    string value = prefix;
    value.reserve(prefix.size() + bytes.size() * 2);
    for (const auto byte : bytes) {
        value.push_back(hexadecimal[(byte >> 4U) & 0x0fU]);
        value.push_back(hexadecimal[byte & 0x0fU]);
    }
    return value;
}

}
