#include "zconsole/runtime/asset_stream.hpp"
#include <filesystem>
#include <fstream>
namespace zconsole {
class FileAssetStream final : public ZAssetStream {
public:
  explicit FileAssetStream(std::filesystem::path root): root_(std::move(root)) {}
  bool open(const std::string& asset, std::string* error) override {
    close(); path_=root_/asset; file_.open(path_,std::ios::binary); if(!file_) { if(error)*error="asset ausente: "+path_.string(); return false; }
    size_=std::filesystem::file_size(path_); return true;
  }
  std::size_t read(void* buffer,std::size_t size) override { if(!file_) return 0; file_.read(static_cast<char*>(buffer),static_cast<std::streamsize>(size)); return static_cast<std::size_t>(file_.gcount()); }
  bool seek(std::uint64_t offset) override { if(!file_) return false; file_.clear(); file_.seekg(static_cast<std::streamoff>(offset)); return static_cast<bool>(file_); }
  std::uint64_t position() const override { return file_?static_cast<std::uint64_t>(const_cast<std::ifstream&>(file_).tellg()):0; }
  std::uint64_t size() const override { return size_; }
  void close() override { if(file_.is_open()) file_.close(); size_=0; }
private: std::filesystem::path root_,path_; std::ifstream file_; std::uint64_t size_=0;
};
std::unique_ptr<ZAssetStream> createFileAssetStream(const std::string& root) { return std::make_unique<FileAssetStream>(root); }
}
