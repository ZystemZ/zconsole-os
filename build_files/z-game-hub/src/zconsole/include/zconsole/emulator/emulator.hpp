#pragma once
#include "zconsole/platform/host.hpp"
#include "zconsole/platform/settings.hpp"
#include "zconsole/runtime/libretro_core.hpp"
#include "zconsole/runtime/rom_runtime.hpp"
#include "zconsole/zcard/virtual_card.hpp"
#include "zconsole/emulator/console.hpp"
#include "zconsole/emulator/zexe.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace zconsole {
enum class CardExecutionKind {
  Unknown,
  Zexe,
  NativeWindows,
  RomInternalCore,
  RomExternalProcess,
  RomWindowsMount,
  RomNoBackend
};

struct GuestExecutionPolicy {
  std::string platform;
  std::string architecture;
  std::string binaryFormat;
  std::string abi;
  std::uint32_t visibleProcessors = 1;
  std::string hostWorkers = "automatic";
  std::string threadPolicy = "serialized_guest_parallel_host";
};

class ZConsoleEmulator {
public:
  ZConsoleEmulator();
  ~ZConsoleEmulator();
  bool loadCard(const std::filesystem::path& path, std::string* error=nullptr);
  bool boot(std::string* error=nullptr);
  bool pause(std::string* error=nullptr);
  bool resume(std::string* error=nullptr);
  bool reset(std::string* error=nullptr);
  bool runInstructions(std::size_t count,std::string* error=nullptr);
  bool runOneInstruction(std::string* error=nullptr);
  void shutdown();
  bool isRunning() const { return running_; }
  bool isLoaded() const { return booted_ && card_ != nullptr; }
  bool hasCpuProgram() const { return executionKind_ == CardExecutionKind::Zexe; }
  CardExecutionKind executionKind() const { return executionKind_; }
  ZConsole& console() { return console_; }
  VirtualZCard* card() { return card_.get(); }
  const std::string& lastError() const { return lastError_; }
  const ZExeHeader& executable() const { return executable_; }
  const std::string& title() const { return title_; }
  const std::string& bootScene() const { return bootScene_; }
  const RomLaunchPlan& romPlan() const { return romPlan_; }
  LibretroCore& internalCore() { return internalCore_; }
  const LibretroCore& internalCore() const { return internalCore_; }
  const GuestExecutionPolicy& guestPolicy() const { return guestPolicy_; }
private:
  std::unique_ptr<ZHost> host_;
  std::unique_ptr<VirtualZCard> card_;
  bool running_ = false;
  bool booted_ = false;
  CardExecutionKind executionKind_ = CardExecutionKind::Unknown;
  ZConsole console_;
  std::string lastError_;
  ZExeHeader executable_{};
  std::string title_;
  std::string bootScene_;
  RomLaunchPlan romPlan_{};
  LibretroCore internalCore_;
  GuestExecutionPolicy guestPolicy_{};
  std::filesystem::path romTempRoot_;
  std::vector<std::uint8_t> loadedExecutable_;
};
}
