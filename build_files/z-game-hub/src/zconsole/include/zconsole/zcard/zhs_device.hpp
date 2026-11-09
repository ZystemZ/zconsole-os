#pragma once
#include "zconsole/zcard/zhs_protocol.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace zconsole {

struct ZhsDeviceInfo {
  std::string deviceId;
  std::string firmwareVersion;
  std::uint64_t capacity = 0;
  std::uint32_t blockSize = 4096;
  bool present = false;
  bool readOnly = true;
  bool masterLocked = true;
};

class ZhsDevice {
public:
  virtual ~ZhsDevice() = default;
  virtual bool open(std::string* error = nullptr) = 0;
  virtual void close() = 0;
  virtual bool isOpen() const = 0;
  virtual ZhsDeviceInfo info() const = 0;
  virtual bool read(std::uint64_t offset, std::vector<std::uint8_t>& data,
                    std::string* error = nullptr) = 0;
  virtual bool verify(std::string* error = nullptr) = 0;
  virtual bool write(std::uint64_t offset, const std::vector<std::uint8_t>& data,
                     std::string* error = nullptr) = 0;
};

// Backend sem hardware para contratos, testes e desenvolvimento do emulador.
// Ele deliberadamente começa em modo somente leitura.
class ZhsMemoryDevice final : public ZhsDevice {
public:
  explicit ZhsMemoryDevice(std::vector<std::uint8_t> image,
                           std::string deviceId = "ZHS-SIM-0001");
  bool open(std::string* error = nullptr) override;
  void close() override;
  bool isOpen() const override { return open_; }
  ZhsDeviceInfo info() const override;
  bool read(std::uint64_t offset, std::vector<std::uint8_t>& data,
            std::string* error = nullptr) override;
  bool verify(std::string* error = nullptr) override;
  bool write(std::uint64_t offset, const std::vector<std::uint8_t>& data,
             std::string* error = nullptr) override;

private:
  std::vector<std::uint8_t> image_;
  std::string deviceId_;
  bool open_ = false;
};

std::unique_ptr<ZhsDevice> createZhsMemoryDevice(std::vector<std::uint8_t> image);

} // namespace zconsole
