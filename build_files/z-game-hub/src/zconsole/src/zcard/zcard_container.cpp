#include "zconsole/zcard/zcard_container.hpp"
#include "zconsole/zcard/zcf_hash.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>

namespace zconsole {
namespace {
std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t p) {
  return static_cast<std::uint16_t>(b[p] | (static_cast<std::uint16_t>(b[p + 1]) << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
  return static_cast<std::uint32_t>(b[p]) |
         (static_cast<std::uint32_t>(b[p + 1]) << 8) |
         (static_cast<std::uint32_t>(b[p + 2]) << 16) |
         (static_cast<std::uint32_t>(b[p + 3]) << 24);
}
std::uint64_t u64(const std::vector<std::uint8_t>& b, std::size_t p) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(b[p + i]) << (8 * i);
  return value;
}
void fail(std::string* error, const std::string& message) { if (error) *error = message; }
std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  std::vector<char> chars((std::istreambuf_iterator<char>(file)), {});
  return std::vector<std::uint8_t>(chars.begin(), chars.end());
}
class RegionStream final : public ZAssetStream {
public:
  explicit RegionStream(std::vector<std::uint8_t> data) : data_(std::move(data)) {}
  bool open(const std::string&, std::string*) override { position_ = 0; return true; }
  std::size_t read(void* buffer, std::size_t size) override {
    const auto start = std::min(position_, data_.size());
    const auto count = std::min(size, data_.size() - start);
    if (count) std::memcpy(buffer, data_.data() + start, count);
    position_ += count;
    return count;
  }
  bool seek(std::uint64_t offset) override {
    if (offset > data_.size()) return false;
    position_ = static_cast<std::size_t>(offset);
    return true;
  }
  std::uint64_t position() const override { return position_; }
  std::uint64_t size() const override { return data_.size(); }
  void close() override { data_.clear(); position_ = 0; }
private:
  std::vector<std::uint8_t> data_;
  std::size_t position_ = 0;
};
}

ZCardContainer::ZCardContainer(std::filesystem::path file) : file_(std::move(file)) {}

bool ZCardContainer::open(std::string* error) {
  regions_.clear();
  masterArchive_.reset();
  zcf_ = false;
  const auto bytes = readAll(file_);
  if (bytes.size() < 4) { fail(error, "cartão inválido: arquivo vazio ou inacessível"); return false; }
  if (std::memcmp(bytes.data(), "ZCF0", 4) != 0) {
    masterArchive_ = std::make_unique<ZCardArchive>(file_);
    return masterArchive_->open(error);
  }
  if (bytes.size() < 64) { fail(error, "cartão corrompido: header ZCF incompleto"); return false; }
  const auto version = u16(bytes, 4);
  const auto headerSize = u16(bytes, 6);
  const auto blockSize = u32(bytes, 12);
  const auto declaredCapacity = u64(bytes, 16);
  const auto count = u32(bytes, 24);
  const auto tableOffset = u32(bytes, 28);
  const std::vector<std::uint8_t> storedHeaderHash(bytes.begin() + 32, bytes.begin() + 64);
  auto headerForHash = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 64);
  std::fill(headerForHash.begin() + 32, headerForHash.end(), 0);
  if (storedHeaderHash != zcfSha256(headerForHash)) { fail(error, "cartão corrompido: hash do header ZCF divergente"); return false; }
  const auto tableEnd = static_cast<std::uint64_t>(tableOffset) + static_cast<std::uint64_t>(count) * 48;
  if (version != 1 || headerSize != 64 || blockSize != 4096 || tableOffset < 64 || tableEnd > bytes.size()) {
    fail(error, "header ZCF incompatível ou tabela fora dos limites"); return false;
  }
  if (declaredCapacity == 0 || declaredCapacity > bytes.size() || tableEnd > static_cast<std::uint64_t>(bytes.size())) {
    fail(error, "capacidade declarada do ZCF inválida"); return false;
  }
  zcf_ = true;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
  std::vector<bool> seenKinds(6, false);
  std::uint64_t summedCapacity = 0;
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto p = static_cast<std::size_t>(tableOffset) + static_cast<std::size_t>(i) * 48;
    ZcfRegion region;
    region.offset = u64(bytes, p); region.size = u64(bytes, p + 8); region.capacity = u64(bytes, p + 16);
    region.kind = static_cast<ZcfRegionKind>(u32(bytes, p + 24)); region.flags = u32(bytes, p + 28);
    region.hashPrefix.assign(bytes.begin() + p + 32, bytes.begin() + p + 48);
    const auto kindValue = static_cast<std::uint32_t>(region.kind);
    if (kindValue < 1 || kindValue > 5 || seenKinds[kindValue]) {
      fail(error, "região ZCF duplicada ou tipo inválido"); return false;
    }
    seenKinds[kindValue] = true;
    if (region.offset % blockSize != 0 || region.offset < tableEnd || region.size > region.capacity || region.offset > bytes.size() || region.capacity > bytes.size() - region.offset) {
      fail(error, "região ZCF fora dos limites ou desalinhada"); return false;
    }
    if (summedCapacity > declaredCapacity || region.capacity > declaredCapacity - summedCapacity) {
      fail(error, "capacidade das regiões ZCF excede a capacidade declarada"); return false;
    }
    summedCapacity += region.capacity;
    ranges.emplace_back(region.offset, region.offset + region.capacity);
    regions_.push_back(std::move(region));
  }
  if (summedCapacity != declaredCapacity) { fail(error, "capacidade das regiões ZCF diverge do header"); return false; }
  std::sort(ranges.begin(), ranges.end());
  for (std::size_t i = 1; i < ranges.size(); ++i) if (ranges[i].first < ranges[i - 1].second) { fail(error, "regiões ZCF sobrepostas"); return false; }
  const auto master = std::find_if(regions_.begin(), regions_.end(), [](const auto& r) { return r.kind == ZcfRegionKind::Master; });
  if (master == regions_.end()) { fail(error, "região MASTER ausente"); return false; }
  const auto masterBytes = std::vector<std::uint8_t>(bytes.begin() + master->offset, bytes.begin() + master->offset + master->size);
  const auto masterHash = zcfSha256(masterBytes);
  if (!std::equal(master->hashPrefix.begin(), master->hashPrefix.end(), masterHash.begin())) { fail(error, "região MASTER inválida: hash divergente"); return false; }
  tempZip_ = std::filesystem::temp_directory_path() / "zconsole_zcf_master.zip";
  std::ofstream output(tempZip_, std::ios::binary | std::ios::trunc);
  if (!output) { fail(error, "não foi possível criar cache temporário da região MASTER"); return false; }
  output.write(reinterpret_cast<const char*>(masterBytes.data()), static_cast<std::streamsize>(masterBytes.size()));
  output.close();
  masterArchive_ = std::make_unique<ZCardArchive>(tempZip_);
  return masterArchive_->open(error);
}

bool ZCardContainer::verify(std::string* error) const {
  if (!zcf_) {
    if (!masterArchive_) { fail(error, "cartão legado não montado"); return false; }
    return true;
  }
  if (regions_.empty()) { fail(error, "container ZCF não montado"); return false; }
  const auto bytes = readAll(file_);
  if (bytes.size() < 64) { fail(error, "arquivo ZCF truncado"); return false; }
  auto header = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 64);
  const auto stored = std::vector<std::uint8_t>(header.begin() + 32, header.end());
  std::fill(header.begin() + 32, header.end(), 0);
  if (stored != zcfSha256(header)) { fail(error, "hash do header ZCF divergente"); return false; }
  for (const auto& region : regions_) {
    if (region.offset > bytes.size() || region.size > bytes.size() - region.offset) { fail(error, "região ZCF truncada"); return false; }
    const auto hash = zcfSha256(std::vector<std::uint8_t>(bytes.begin() + region.offset, bytes.begin() + region.offset + region.size));
    if (region.hashPrefix.size() != 16 || !std::equal(region.hashPrefix.begin(), region.hashPrefix.end(), hash.begin())) {
      fail(error, "hash divergente em região ZCF"); return false;
    }
  }
  return masterArchive_ && masterArchive_->open(error);
}

std::vector<std::string> ZCardContainer::entries() const { return masterArchive_ ? masterArchive_->entries() : std::vector<std::string>{}; }

std::unique_ptr<ZAssetStream> ZCardContainer::openStream(const std::string& asset, std::string* error) const {
  if (!masterArchive_) { fail(error, "container ZCF não montado"); return nullptr; }
  const auto found = std::find_if(regions_.begin(), regions_.end(), [&](const auto& region) {
    const char* name = region.kind == ZcfRegionKind::Update ? "UPDATE" : region.kind == ZcfRegionKind::Dlc ? "DLC" : region.kind == ZcfRegionKind::Save ? "SAVE" : region.kind == ZcfRegionKind::User ? "USER" : "MASTER";
    return asset == name;
  });
  if (found != regions_.end() && found->kind != ZcfRegionKind::Master) {
    const auto bytes = readAll(file_);
    if (found->offset > bytes.size() || found->size > bytes.size() - found->offset) { fail(error, "região ZCF truncada"); return nullptr; }
    return std::make_unique<RegionStream>(std::vector<std::uint8_t>(bytes.begin() + found->offset, bytes.begin() + found->offset + found->size));
  }
  return masterArchive_->openStream(asset, error);
}

bool ZCardContainer::writeStream(ZcfRegionKind region, const std::string&, const std::vector<std::uint8_t>& data, std::string* error) {
  if (!zcf_) { fail(error, "escrita de região requer container ZCF"); return false; }
  if (region == ZcfRegionKind::Master) { fail(error, "escrita proibida na região MASTER"); return false; }
  const auto all = readAll(file_);
  const auto found = std::find_if(regions_.begin(), regions_.end(), [&](const auto& item) { return item.kind == region; });
  if (found == regions_.end()) { fail(error, "região ZCF não encontrada"); return false; }
  if (data.size() > found->capacity) { fail(error, "dados excedem a capacidade da região"); return false; }
  auto bytes = all;
  std::copy(data.begin(), data.end(), bytes.begin() + found->offset);
  std::fill(bytes.begin() + found->offset + data.size(), bytes.begin() + found->offset + found->capacity, 0);
  const auto hash = zcfSha256(data);
  const auto index = static_cast<std::size_t>(std::distance(regions_.begin(), found));
  const auto table = static_cast<std::size_t>(64) + index * 48;
  for (int i = 0; i < 8; ++i) bytes[table + 8 + i] = static_cast<std::uint8_t>(data.size() >> (8 * i));
  std::copy(hash.begin(), hash.begin() + 16, bytes.begin() + table + 32);
  auto header = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 64);
  std::fill(header.begin() + 32, header.end(), 0);
  const auto headerHash = zcfSha256(header);
  std::copy(headerHash.begin(), headerHash.end(), bytes.begin() + 32);
  const auto temporary = file_.string() + ".tmp";
  { std::ofstream output(temporary, std::ios::binary | std::ios::trunc); if (!output) { fail(error, "não foi possível abrir escrita temporária"); return false; } output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); if (!output) { fail(error, "falha ao gravar escrita temporária"); return false; } }
  std::error_code ec; std::filesystem::rename(temporary, file_, ec);
  if (ec) { std::filesystem::remove(file_, ec); std::filesystem::rename(temporary, file_, ec); }
  if (ec) { fail(error, "falha ao substituir container ZCF"); return false; }
  found->size = data.size(); found->hashPrefix.assign(hash.begin(), hash.begin() + 16);
  return true;
}
}
