#pragma once
#include "zconsole/asset_runtime.hpp"
#include "zconsole/runtime/asset_stream.hpp"
#include "zconsole/zcard/zcard_archive.hpp"
#include "zconsole/zcard/zcard_container.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace zconsole {
class VirtualZCard {
public:
  explicit VirtualZCard(std::filesystem::path root);
  bool mount(std::string* error=nullptr);
  bool verify(std::string* error=nullptr);
  std::unique_ptr<ZAssetStream> openStream(const std::string& asset, std::string* error=nullptr) const;
  std::vector<std::string> entries(std::string* error=nullptr) const;
  bool writeStream(ZcfRegionKind region,const std::string& asset,const std::vector<std::uint8_t>& data,std::string* error=nullptr);
  AssetRuntime& runtime() { return runtime_; }
  const std::filesystem::path& root() const { return root_; }
private:
  std::filesystem::path root_;
  AssetRuntime runtime_;
  std::unique_ptr<ZCardArchive> archive_;
  std::unique_ptr<ZCardContainer> container_;
  bool mounted_ = false;
};
}
