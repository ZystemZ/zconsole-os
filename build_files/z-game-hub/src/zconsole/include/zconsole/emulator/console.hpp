#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "zconsole/emulator/cpu.hpp"

namespace zconsole {
class ZConsoleMemory {
public:
  explicit ZConsoleMemory(std::size_t sizeBytes = 64 * 1024 * 1024);
  std::size_t size() const { return data_.size(); }
  bool read(std::uint64_t address, void* buffer, std::size_t size) const;
  bool write(std::uint64_t address, const void* buffer, std::size_t size);
  void clear();
private: std::vector<std::uint8_t> data_;
};
class ZConsoleTiming {
public:
  void reset(); void advanceUs(std::uint64_t microseconds);
  std::uint64_t totalUs() const { return totalUs_; } double frameTimeMs() const { return frameTimeMs_; }
private: std::uint64_t totalUs_ = 0; double frameTimeMs_ = 16.666;
};
class ZConsole {
public:
  explicit ZConsole(std::size_t memoryBytes = 64 * 1024 * 1024);
  void reset(); void tick(std::uint64_t microseconds);
  bool running() const { return running_; } void setRunning(bool value) { running_ = value; }
  ZConsoleMemory& memory() { return memory_; } const ZConsoleTiming& timing() const { return timing_; }
  ZVirtualCpu& cpu() { return cpu_; } const ZVirtualCpu& cpu() const { return cpu_; }
private:
  ZConsoleMemory memory_; ZConsoleTiming timing_; ZVirtualCpu cpu_; bool running_ = false;
};
}
