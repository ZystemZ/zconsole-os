#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zconsole {

enum class ZhsCommand : std::uint8_t {
  GetInfo = 0x01,
  Read = 0x02,
  Verify = 0x03,
  GetStatus = 0x04,
};

enum class ZhsStatus : std::uint8_t {
  Ok = 0,
  InvalidFrame = 1,
  UnsupportedVersion = 2,
  InvalidCommand = 3,
  InvalidRange = 4,
  ReadOnly = 5,
  NotPresent = 6,
  IntegrityError = 7,
  IoError = 8,
};

struct ZhsFrame {
  static constexpr std::uint16_t Magic = 0x485A; // wire bytes: 'Z', 'H'
  static constexpr std::uint8_t Version = 1;

  ZhsCommand command = ZhsCommand::GetStatus;
  std::uint16_t flags = 0;
  std::uint16_t requestId = 0;
  std::uint64_t offset = 0;
  std::vector<std::uint8_t> payload;
};

// Frame format, all integer fields little-endian:
// magic:u16, version:u8, command:u8, flags:u16, request_id:u16,
// offset:u64, length:u32, payload:length, crc32:u32.
std::vector<std::uint8_t> encodeZhsFrame(const ZhsFrame& frame);
bool decodeZhsFrame(const std::vector<std::uint8_t>& bytes, ZhsFrame& frame,
                    std::string* error = nullptr);
std::uint32_t zhsCrc32(const std::uint8_t* data, std::size_t size);

} // namespace zconsole
