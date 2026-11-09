#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace zconsole {
class ZAssetStream {
public:
  virtual ~ZAssetStream() = default;
  virtual bool open(const std::string& asset, std::string* error=nullptr) = 0;
  virtual std::size_t read(void* buffer, std::size_t size) = 0;
  virtual bool seek(std::uint64_t offset) = 0;
  virtual std::uint64_t position() const = 0;
  virtual std::uint64_t size() const = 0;
  virtual void close() = 0;
};
std::unique_ptr<ZAssetStream> createFileAssetStream(const std::string& root);
}
