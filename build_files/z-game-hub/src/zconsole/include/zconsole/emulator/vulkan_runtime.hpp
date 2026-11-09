#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace zconsole {
class ZSdlRuntime;
struct ZVulkanDeviceInfo { std::string name; std::uint32_t vendorId=0,deviceId=0; std::uint32_t apiMajor=0,apiMinor=0; bool discrete=false; };
class ZVulkanRuntime { public: struct Impl; ZVulkanRuntime(); ~ZVulkanRuntime(); bool initialize(bool validation=false,std::string* error=nullptr); bool initializeForSdl(const ZSdlRuntime& sdl,bool validation=false,std::string* error=nullptr); std::vector<ZVulkanDeviceInfo> devices() const; std::string diagnostics() const; bool presentFrame(std::string* error=nullptr); void shutdown(); bool initialized()const{return initialized_;} bool presentationReady()const; private: std::unique_ptr<Impl> impl_; bool initialized_=false; };
}
