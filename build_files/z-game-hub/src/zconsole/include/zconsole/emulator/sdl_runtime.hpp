#pragma once
#include "zconsole/emulator/hardware.hpp"
#include "zconsole/platform/services.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace zconsole {
struct ZSdlConfig { std::uint32_t width=1280,height=720; std::uint32_t logicalWidth=640,logicalHeight=360; bool audio=true; bool vsync=true; ZAudioConfig audioConfig{}; };
class ZSdlRuntime { public: ZSdlRuntime(); ~ZSdlRuntime(); bool initialize(const ZSdlConfig& config={},std::string* error=nullptr); bool pumpInput(ZConsoleInput& input,std::string* error=nullptr); bool present(const ZConsoleGraphics& graphics,std::string* error=nullptr); bool playTone(const ZTone& tone,std::string* error=nullptr); std::vector<ZControllerInfo> controllers() const; std::vector<std::string> requiredVulkanExtensions(std::string* error=nullptr) const; void* nativeWindow() const; void shutdown(); bool initialized()const{return initialized_;} private: struct Impl; std::unique_ptr<Impl> impl_; bool initialized_=false; };
}
