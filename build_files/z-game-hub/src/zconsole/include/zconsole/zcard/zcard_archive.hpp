#pragma once
#include "zconsole/runtime/asset_stream.hpp"
#include <filesystem>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace zconsole {
class ZCardArchive {
public:
  explicit ZCardArchive(std::filesystem::path file);
  bool open(std::string* error=nullptr);
  std::unique_ptr<ZAssetStream> openStream(const std::string& asset,std::string* error=nullptr) const;
  std::vector<std::string> entries() const;
private:
  std::filesystem::path file_;
  struct Entry { std::uint32_t localOffset=0, compressedSize=0, uncompressedSize=0, crc=0; std::uint16_t method=0; };
  std::map<std::string,Entry> entries_;
};
}
