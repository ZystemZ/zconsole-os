#include "zconsole/zcard/zcard_archive.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>
#ifdef ZCARD_HAS_ZLIB
#include <zlib.h>
#endif

namespace zconsole {
namespace {
constexpr std::uint64_t kMaxEntryBytes = 256ull * 1024ull * 1024ull;
std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::vector<char> chars((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::vector<std::uint8_t> bytes; bytes.reserve(chars.size());
  for (char value : chars) bytes.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(value)));
  return bytes;
}
std::uint16_t readU16(const std::vector<std::uint8_t>& b, std::size_t p) { return static_cast<std::uint16_t>(b[p] | (b[p+1] << 8)); }
std::uint32_t readU32(const std::vector<std::uint8_t>& b, std::size_t p) { return b[p] | (b[p+1] << 8) | (b[p+2] << 16) | (b[p+3] << 24); }
std::uint32_t crc32(const std::vector<std::uint8_t>& data) {
  std::uint32_t crc=0xffffffffu; for (const auto value : data) { crc^=value; for (int i=0;i<8;++i) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u))); } return ~crc;
}
bool safeName(const std::string& name) {
  if (name.empty() || name.front()=='/' || name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos) return false;
  std::size_t begin=0; while(begin<=name.size()) { const auto end=name.find('/',begin); const auto part=name.substr(begin,end==std::string::npos?std::string::npos:end-begin); if(part==".." || (part.empty() && end!=std::string::npos)) return false; if(end==std::string::npos) break; begin=end+1; } return true;
}
void fail(std::string* error,const std::string& message) { if(error)*error=message; }
}
class ArchiveStream final : public ZAssetStream {
public:
  explicit ArchiveStream(std::vector<std::uint8_t> data) : data_(std::move(data)) {}
  bool open(const std::string&, std::string*) override { position_=0; return true; }
  std::size_t read(void* buffer,std::size_t size) override { const auto available=data_.size()-std::min(position_,data_.size()); const auto count=std::min(size,available); if(count){std::memcpy(buffer,data_.data()+position_,count);position_+=count;} return count; }
  bool seek(std::uint64_t offset) override { if(offset>data_.size())return false;position_=static_cast<std::size_t>(offset);return true; }
  std::uint64_t position() const override { return position_; }
  std::uint64_t size() const override { return data_.size(); }
  void close() override { data_.clear();position_=0; }
private: std::vector<std::uint8_t> data_; std::size_t position_=0;
};
ZCardArchive::ZCardArchive(std::filesystem::path file):file_(std::move(file)){}
bool ZCardArchive::open(std::string* error) {
  const auto bytes=readAll(file_); if(bytes.size()<22){fail(error,"ZCARD inválido: arquivo muito pequeno");return false;}
  const auto searchStart=bytes.size()>65557?bytes.size()-65557:0; std::size_t eocd=bytes.size()-22; while(eocd>=searchStart&&readU32(bytes,eocd)!=0x06054b50u){if(eocd==0)break;--eocd;} if(readU32(bytes,eocd)!=0x06054b50u){fail(error,"assinatura ZIP ausente");return false;}
  const auto disk=readU16(bytes,eocd+4), centralDisk=readU16(bytes,eocd+6); const auto count=readU16(bytes,eocd+10); const auto directorySize=readU32(bytes,eocd+12); const auto directoryOffset=readU32(bytes,eocd+16);
  if(disk!=0||centralDisk!=0){fail(error,"ZCARD dividido em múltiplos discos não suportado");return false;} if(count==0xffff||directorySize==0xffffffffu||directoryOffset==0xffffffffu){fail(error,"ZIP64 não suportado");return false;} if(static_cast<std::uint64_t>(directoryOffset)+directorySize>bytes.size()){fail(error,"diretório ZIP inválido");return false;}
  entries_.clear(); std::size_t p=directoryOffset; for(std::uint16_t i=0;i<count;++i){
    if(p+46>bytes.size()||readU32(bytes,p)!=0x02014b50u){fail(error,"entrada ZIP inválida");return false;} const auto flags=readU16(bytes,p+8); const auto method=readU16(bytes,p+10); const auto crc=readU32(bytes,p+16); const auto compressed=readU32(bytes,p+20); const auto uncompressed=readU32(bytes,p+24); const auto nameLength=readU16(bytes,p+28); const auto extraLength=readU16(bytes,p+30); const auto commentLength=readU16(bytes,p+32); const auto localOffset=readU32(bytes,p+42);
    if(compressed==0xffffffffu||uncompressed==0xffffffffu||localOffset==0xffffffffu){fail(error,"ZIP64 não suportado");return false;} if((flags&1)!=0){fail(error,"entrada ZIP criptografada não suportada");return false;} if(uncompressed>kMaxEntryBytes||compressed>kMaxEntryBytes){fail(error,"entrada ZCARD excede o limite de tamanho");return false;} if(p+46+nameLength+extraLength+commentLength>bytes.size()){fail(error,"nome ZIP inválido");return false;}
    const std::string name(reinterpret_cast<const char*>(bytes.data()+p+46),nameLength); if(!safeName(name)){fail(error,"path traversal ou nome inseguro no ZCARD");return false;} entries_[name]=Entry{localOffset,compressed,uncompressed,crc,method}; p+=46+nameLength+extraLength+commentLength;
  } return true;
}
std::vector<std::string> ZCardArchive::entries() const { std::vector<std::string> result; result.reserve(entries_.size()); for(const auto& item:entries_)result.push_back(item.first); return result; }
std::unique_ptr<ZAssetStream> ZCardArchive::openStream(const std::string& asset,std::string* error) const {
  if(!safeName(asset)){fail(error,"nome de asset inseguro");return nullptr;} const auto found=entries_.find(asset); if(found==entries_.end()){fail(error,"asset não encontrado no ZCARD");return nullptr;}
  const auto all=readAll(file_); const auto& entry=found->second; if(entry.localOffset>all.size()||all.size()-entry.localOffset<30||readU32(all,entry.localOffset)!=0x04034b50u){fail(error,"header local ZIP inválido");return nullptr;}
  const auto nameLength=readU16(all,entry.localOffset+26), extraLength=readU16(all,entry.localOffset+28); const auto dataOffset=static_cast<std::size_t>(entry.localOffset)+30+nameLength+extraLength; if(dataOffset>all.size()||entry.compressedSize>all.size()-dataOffset){fail(error,"payload ZIP inválido");return nullptr;}
  std::vector<std::uint8_t> output; if(entry.method==0) output.assign(all.begin()+dataOffset,all.begin()+dataOffset+entry.compressedSize);
#ifdef ZCARD_HAS_ZLIB
  else if(entry.method==8){output.resize(entry.uncompressedSize);z_stream stream{};stream.next_in=const_cast<Bytef*>(all.data()+dataOffset);stream.avail_in=entry.compressedSize;stream.next_out=output.data();stream.avail_out=entry.uncompressedSize;const int init=inflateInit2(&stream,-MAX_WBITS);const int result=init==Z_OK?inflate(&stream,Z_FINISH):Z_DATA_ERROR;if(init!=Z_OK||result!=Z_STREAM_END){fail(error,"falha ao descompactar ZCARD");if(init==Z_OK)inflateEnd(&stream);return nullptr;}inflateEnd(&stream);}
#endif
  else {fail(error,"método ZIP não suportado; habilite ZLIB para DEFLATE");return nullptr;}
  if(output.size()!=entry.uncompressedSize||crc32(output)!=entry.crc){fail(error,"CRC32 divergente no asset ZCARD");return nullptr;} auto result=std::make_unique<ArchiveStream>(std::move(output)); result->open(asset,nullptr); return result;
}
}
