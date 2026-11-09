#pragma once
#include "zconsole/runtime/asset_stream.hpp"
#include "zconsole/zcard/zcard_archive.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace zconsole {
enum class ZcfRegionKind : std::uint32_t { Master=1, Update=2, Dlc=3, Save=4, User=5 };
struct ZcfRegion { std::uint64_t offset=0,size=0,capacity=0; ZcfRegionKind kind=ZcfRegionKind::Master; std::uint32_t flags=0; std::vector<std::uint8_t> hashPrefix; };
class ZCardContainer {
public:
  explicit ZCardContainer(std::filesystem::path file);
  bool open(std::string* error=nullptr);
  bool verify(std::string* error=nullptr) const;
  std::vector<ZcfRegion> regions() const { return regions_; }
  bool isZcf() const { return zcf_; }
  std::vector<std::string> entries() const;
  std::unique_ptr<ZAssetStream> openStream(const std::string& asset,std::string* error=nullptr) const;
  bool writeStream(ZcfRegionKind region,const std::string& asset,const std::vector<std::uint8_t>& data,std::string* error=nullptr);
private:
  std::filesystem::path file_; bool zcf_=false; std::vector<ZcfRegion> regions_; std::unique_ptr<ZCardArchive> masterArchive_; std::filesystem::path tempZip_;
};
}
