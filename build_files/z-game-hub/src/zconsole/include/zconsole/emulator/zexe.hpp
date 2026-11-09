#pragma once
#include "zconsole/emulator/console.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zconsole {
struct ZExeHeader {
  std::uint16_t version = 1;
  std::uint16_t architecture = 1; // 1 = ZCPU-32 little-endian
  std::uint32_t headerSize = 40;
  std::uint32_t entryPoint = 0;
  std::uint32_t codeOffset = 40;
  std::uint32_t codeSize = 0;
  std::uint32_t dataOffset = 0;
  std::uint32_t dataSize = 0;
  std::uint32_t memoryRequired = 0;
  std::uint32_t checksum = 0;
};

class ZExecutableLoader {
public:
  static std::vector<std::uint8_t> buildMinimal(std::uint32_t storeAddress = 0x100);
  static bool inspect(const std::vector<std::uint8_t>& bytes, ZExeHeader& header, std::string* error = nullptr);
  static bool load(const std::vector<std::uint8_t>& bytes, ZConsole& console, ZExeHeader* header = nullptr, std::string* error = nullptr);
};
}

// Binary format ZEXE-0.1: magic "ZEXE", followed by ten little-endian
// uint32/uint16 fields represented by ZExeHeader. Code is a flat segment.
// The checksum is the unsigned byte sum of all bytes after the header.
// entryPoint and segment offsets are virtual-memory addresses/file offsets respectively.

inline constexpr std::uint16_t ZEXE_ARCH_ZCPU32 = 1;
inline constexpr std::size_t ZEXE_HEADER_SIZE = 40;
inline constexpr std::uint8_t ZEXE_OP_LOAD_IMM = 0x01;
inline constexpr std::uint8_t ZEXE_OP_ADD_IMM = 0x02;
inline constexpr std::uint8_t ZEXE_OP_STORE = 0x03;
inline constexpr std::uint8_t ZEXE_OP_JMP = 0x04;
inline constexpr std::uint8_t ZEXE_OP_CMP_IMM = 0x05;
inline constexpr std::uint8_t ZEXE_OP_JZ = 0x06;
inline constexpr std::uint8_t ZEXE_OP_PUSH_A = 0x07;
inline constexpr std::uint8_t ZEXE_OP_POP_A = 0x08;
inline constexpr std::uint8_t ZEXE_OP_CALL = 0x09;
inline constexpr std::uint8_t ZEXE_OP_RET = 0x0a;
inline constexpr std::uint8_t ZEXE_OP_HALT = 0xff;

namespace zconsole {
class ZCardBuilder {
public:
  static bool build(const std::filesystem::path& source, const std::filesystem::path& output, std::string* report = nullptr, std::string* error = nullptr);
  static bool buildZcf(const std::filesystem::path& source, const std::filesystem::path& output, std::string* report = nullptr, std::string* error = nullptr);
  static bool buildZuf(const std::filesystem::path& source, const std::filesystem::path& output, std::string* report = nullptr, std::string* error = nullptr);
};
}
