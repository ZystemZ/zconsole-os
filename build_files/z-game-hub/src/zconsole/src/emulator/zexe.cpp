#include "zconsole/emulator/zexe.hpp"
#include "zconsole/zcard/virtual_card.hpp"
#include "zconsole/zcard/zcard_archive.hpp"
#include "zconsole/zcard/zcf_hash.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace zconsole {
namespace {
void put16(std::vector<std::uint8_t>& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
void put16At(std::vector<std::uint8_t>& b,std::size_t p,std::uint16_t v){b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8);}
void put32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(v>>(i*8)));}
void put32At(std::vector<std::uint8_t>& b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>(v>>(i*8));}
void put64At(std::vector<std::uint8_t>& b,std::size_t p,std::uint64_t v){for(int i=0;i<8;++i)b[p+i]=static_cast<std::uint8_t>(v>>(i*8));}
std::uint16_t get16(const std::vector<std::uint8_t>& b,std::size_t p){return static_cast<std::uint16_t>(b[p]|(b[p+1]<<8));}
std::uint32_t get32(const std::vector<std::uint8_t>& b,std::size_t p){return b[p]|(b[p+1]<<8)|(b[p+2]<<16)|(b[p+3]<<24);}
std::uint32_t sum(const std::vector<std::uint8_t>& b,std::size_t p){std::uint32_t x=0;for(;p<b.size();++p)x+=b[p];return x;}
std::uint32_t hash32(const std::vector<std::uint8_t>& data){std::uint32_t h=2166136261u;for(const auto x:data){h^=x;h*=16777619u;}return h;}
void fail(std::string* e,const std::string& s){if(e)*e=s;}
void zip16(std::ofstream& f,std::uint16_t v){const char x[2]={static_cast<char>(v),static_cast<char>(v>>8)};f.write(x,2);}
void zip32(std::ofstream& f,std::uint32_t v){const char x[4]={static_cast<char>(v),static_cast<char>(v>>8),static_cast<char>(v>>16),static_cast<char>(v>>24)};f.write(x,4);}
std::string value(const std::string& json,const std::string& key){const auto p=json.find("\""+key+"\"");if(p==std::string::npos)return{};const auto c=json.find(':',p);if(c==std::string::npos)return{};const auto q=json.find('"',c+1);if(q==std::string::npos)return{};const auto r=json.find('"',q+1);return r==std::string::npos?std::string{}:json.substr(q+1,r-q-1);}
std::string inferredType(const std::string& path){const auto dot=path.find_last_of('.');if(dot==std::string::npos||dot+1>=path.size())return "binary";std::string type=path.substr(dot+1);std::transform(type.begin(),type.end(),type.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return type;}
}
std::vector<std::uint8_t> ZExecutableLoader::buildMinimal(std::uint32_t storeAddress){std::vector<std::uint8_t> code={0x01,42,0,0,0,0x02,5,0,0,0,0x03};for(int i=0;i<4;++i)code.push_back(static_cast<std::uint8_t>(storeAddress>>(i*8)));code.push_back(0xff);std::vector<std::uint8_t> out={'Z','E','X','E'};put16(out,1);put16(out,ZEXE_ARCH_ZCPU32);put32(out,ZEXE_HEADER_SIZE);put32(out,0);put32(out,ZEXE_HEADER_SIZE);put32(out,static_cast<std::uint32_t>(code.size()));put32(out,0);put32(out,0);put32(out,storeAddress+4);put32(out,sum(code,0));out.insert(out.end(),code.begin(),code.end());return out;}
bool ZExecutableLoader::inspect(const std::vector<std::uint8_t>& b,ZExeHeader& h,std::string* e){if(b.size()<ZEXE_HEADER_SIZE||std::string(reinterpret_cast<const char*>(b.data()),4)!="ZEXE"){fail(e,"ZEXE inválido: assinatura ausente");return false;}h.version=get16(b,4);h.architecture=get16(b,6);h.headerSize=get32(b,8);h.entryPoint=get32(b,12);h.codeOffset=get32(b,16);h.codeSize=get32(b,20);h.dataOffset=get32(b,24);h.dataSize=get32(b,28);h.memoryRequired=get32(b,32);h.checksum=get32(b,36);if(h.version!=1||h.architecture!=ZEXE_ARCH_ZCPU32||h.headerSize!=ZEXE_HEADER_SIZE){fail(e,"ZEXE incompatível: versão ou arquitetura");return false;}if(h.codeOffset<h.headerSize||static_cast<std::uint64_t>(h.codeOffset)+h.codeSize>b.size()||h.entryPoint>=h.codeSize){fail(e,"ZEXE inválido: segmento CODE ou entry point");return false;}if(sum(std::vector<std::uint8_t>(b.begin()+h.codeOffset,b.begin()+h.codeOffset+h.codeSize),0)!=h.checksum){fail(e,"ZEXE inválido: checksum divergente");return false;}return true;}
bool ZExecutableLoader::load(const std::vector<std::uint8_t>& b,ZConsole& c,ZExeHeader* out,std::string* e){ZExeHeader h;if(!inspect(b,h,e))return false;if(h.memoryRequired>c.memory().size()){fail(e,"memória insuficiente para ZEXE");return false;}if(!c.memory().write(0,b.data()+h.codeOffset,h.codeSize)){fail(e,"falha ao carregar CODE na memória");return false;}c.cpu().reset();if(out)*out=h;return true;}

bool ZCardBuilder::build(const std::filesystem::path& source,const std::filesystem::path& output,std::string* report,std::string* error){
  if(!std::filesystem::is_directory(source)){fail(error,"fonte ZCARD não é um diretório: "+source.string());return false;}
  const auto gameMeta=source/"META"/"zgame.json";const auto romMeta=source/"META"/"rom-runtime.json";const auto exe=source/"CODE"/"game.zexe";
  if(!std::filesystem::is_regular_file(gameMeta)&&!std::filesystem::is_regular_file(romMeta)){fail(error,"fonte ZCARD requer META/zgame.json ou META/rom-runtime.json");return false;}
  bool isRom=false;bool isNative=false;ZExeHeader header{};
  if(std::filesystem::is_regular_file(gameMeta)){
    std::ifstream mf(gameMeta);const std::string manifest((std::istreambuf_iterator<char>(mf)),std::istreambuf_iterator<char>());
    if(value(manifest,"format")=="zgame-rom-runtime-0.1"||value(manifest,"format")=="zcard-rom-runtime-0.1")isRom=true;
    else if(value(manifest,"format")=="zgame-native-0.1"||value(manifest,"format")=="zgame-native-0.2"||value(manifest,"platform")=="windows-x64")isNative=true;
    else if(value(manifest,"format")!="zgame-0.1"||value(manifest,"platform")!="zconsole-v1"||value(manifest,"executable")!="CODE/game.zexe"){fail(error,"manifesto incompatível: esperado zgame-0.1, zconsole-v1 e CODE/game.zexe");return false;}
  }
  if(std::filesystem::is_regular_file(romMeta))isRom=true;
  if(!isRom&&!isNative){
    if(!std::filesystem::is_regular_file(exe)){fail(error,"projeto ZEXE requer CODE/game.zexe");return false;}
    std::ifstream ef(exe,std::ios::binary);const std::vector<std::uint8_t> exeBytes((std::istreambuf_iterator<char>(ef)),std::istreambuf_iterator<char>());if(!ZExecutableLoader::inspect(exeBytes,header,error))return false;
  } else if(isRom) {
    std::ifstream rm(romMeta);const std::string runtime((std::istreambuf_iterator<char>(rm)),std::istreambuf_iterator<char>());const auto rom=value(runtime,"rom");if(rom.empty()||rom.front()=='/'||rom.find("..")!=std::string::npos){fail(error,"manifesto ROM inválido: caminho rom ausente ou inseguro");return false;}if(!std::filesystem::is_regular_file(source/rom)){fail(error,"ROM ausente: "+(source/rom).string());return false;}
  }
  std::vector<std::pair<std::string,std::vector<std::uint8_t>>> files;std::unordered_set<std::string> seen;std::uint64_t totalBytes=0;
  for(auto it=std::filesystem::recursive_directory_iterator(source);it!=std::filesystem::recursive_directory_iterator();++it) if(it->is_regular_file()){
    const auto rel=std::filesystem::relative(it->path(),source).generic_string();std::string key=rel;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(rel.empty()||rel.front()=='/'||rel.find("..")!=std::string::npos||!seen.insert(key).second){fail(error,"caminho inválido ou entrada duplicada no projeto ZCARD: "+rel);return false;}
    if(rel.size()>std::numeric_limits<std::uint16_t>::max()){fail(error,"nome de entrada excede o limite ZIP");return false;}
    std::ifstream f(it->path(),std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());if(data.size()>std::numeric_limits<std::uint32_t>::max()){fail(error,"arquivo excede o limite ZIP32");return false;}totalBytes+=data.size();files.emplace_back(rel,std::move(data));
  }
  bool hasAssetManifest=false;for(const auto& item:files)if(item.first=="META/assets.zgf")hasAssetManifest=true;
  if(!hasAssetManifest){std::string generated="ZGF-0.1\n";for(const auto& item:files)if(item.first.rfind("assets/",0)==0){generated+="asset|"+item.first+"|streaming|100|-|-|"+inferredType(item.first)+"\n";}if(generated!="ZGF-0.1\n"){totalBytes+=generated.size();files.emplace_back("META/assets.zgf",std::vector<std::uint8_t>(generated.begin(),generated.end()));}}
  std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  if(files.size()>std::numeric_limits<std::uint16_t>::max()){fail(error,"quantidade de entradas excede o limite ZIP");return false;}
  std::ofstream out(output,std::ios::binary);if(!out){fail(error,"não foi possível criar ZCARD");return false;}
  struct Central{std::string name;std::uint32_t offset,crc,size;};std::vector<Central> central;
  for(const auto& item:files){const auto offset=static_cast<std::uint32_t>(out.tellp());const auto size=static_cast<std::uint32_t>(item.second.size());const auto nameSize=static_cast<std::uint16_t>(item.first.size());std::uint32_t crc=0xffffffffu;for(const auto byte:item.second){crc^=byte;for(int i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}crc=~crc;zip32(out,0x04034b50);zip16(out,20);zip16(out,0);zip16(out,0);zip16(out,0);zip16(out,0);zip32(out,crc);zip32(out,size);zip32(out,size);zip16(out,nameSize);zip16(out,0);out.write(item.first.data(),static_cast<std::streamsize>(item.first.size()));out.write(reinterpret_cast<const char*>(item.second.data()),static_cast<std::streamsize>(item.second.size()));central.push_back({item.first,offset,crc,size});}
  const auto cd=static_cast<std::uint32_t>(out.tellp());for(const auto& item:central){const auto nameSize=static_cast<std::uint16_t>(item.name.size());zip32(out,0x02014b50);zip16(out,20);zip16(out,20);zip16(out,0);zip16(out,0);zip16(out,0);zip16(out,0);zip32(out,item.crc);zip32(out,item.size);zip32(out,item.size);zip16(out,nameSize);zip16(out,0);zip16(out,0);zip16(out,0);zip16(out,0);zip32(out,0);zip32(out,item.offset);out.write(item.name.data(),static_cast<std::streamsize>(item.name.size()));}const auto end=static_cast<std::uint32_t>(out.tellp());zip32(out,0x06054b50);zip16(out,0);zip16(out,0);zip16(out,static_cast<std::uint16_t>(central.size()));zip16(out,static_cast<std::uint16_t>(central.size()));zip32(out,end-cd);zip32(out,cd);zip16(out,0);out.close();
  ZCardArchive verifyArchive(output);std::string verifyError;if(!verifyArchive.open(&verifyError)){fail(error,"ZCARD criado mas falhou na verificação: "+verifyError);return false;}const auto verified=verifyArchive.entries();if(verified.size()!=files.size()){fail(error,"ZCARD criado mas a contagem de entradas diverge");return false;}for(std::size_t i=0;i<files.size();++i){if(verified[i]!=files[i].first){fail(error,"ZCARD criado mas a ordenação diverge na entrada "+std::to_string(i));return false;}auto stream=verifyArchive.openStream(files[i].first,&verifyError);if(!stream||stream->size()!=files[i].second.size()){fail(error,"ZCARD criado mas a entrada não pode ser relida: "+files[i].first);return false;}}
  if(report){std::ostringstream details;for(const auto& item:files)details<<item.first<<" | bytes="<<item.second.size()<<" | fnv1a32="<<std::hex<<hash32(item.second)<<std::dec<<"\n";std::ostringstream r;r<<"ZCARD criado: "<<output.string()<<"\nentradas="<<files.size()<<" bytes="<<totalBytes<<"\n"<<(isRom?"ROM runtime":(isNative?"payload nativo":"ZEXE code="+std::to_string(header.codeSize)+" entrypoint="+std::to_string(header.entryPoint)))<<"\nverificacao=ok\n"<<details.str();*report=r.str();}
  return true;
}
bool ZCardBuilder::buildZcf(const std::filesystem::path& source,const std::filesystem::path& output,std::string* report,std::string* error){
  const auto temporary=std::filesystem::temp_directory_path()/"zconsole_zcf_master_build.zip";std::string zipReport;
  if(!build(source,temporary,&zipReport,error))return false;
  std::ifstream input(temporary,std::ios::binary);std::vector<std::uint8_t> zip((std::istreambuf_iterator<char>(input)),{});input.close();std::error_code ec;std::filesystem::remove(temporary,ec);
  constexpr std::uint32_t block=4096;const std::uint64_t masterOffset=block;const std::uint64_t masterSize=zip.size();const std::uint64_t masterCapacity=((masterSize+block-1)/block)*block;constexpr std::uint64_t writableCapacity=64*1024;
  std::vector<std::uint8_t> header(64,0);header[0]='Z';header[1]='C';header[2]='F';header[3]='0';header[4]=1;header[6]=64;put32At(header,12,block);const auto totalCapacity=masterCapacity+writableCapacity*4;for(int i=0;i<8;++i)header[16+i]=static_cast<std::uint8_t>(totalCapacity>>(8*i));put32At(header,24,5);put32At(header,28,64);
  const auto masterHash=zcfSha256(zip);const auto emptyHash=zcfSha256({});std::vector<std::uint8_t> table(48*5,0);const ZcfRegionKind kinds[5]={ZcfRegionKind::Master,ZcfRegionKind::Update,ZcfRegionKind::Dlc,ZcfRegionKind::Save,ZcfRegionKind::User};std::uint64_t offset=masterOffset;for(int index=0;index<5;++index){auto* entry=table.data()+index*48;const auto capacity=index==0?masterCapacity:writableCapacity;const auto size=index==0?masterSize:0;for(int i=0;i<8;++i)entry[i]=static_cast<std::uint8_t>(offset>>(8*i));for(int i=0;i<8;++i)entry[8+i]=static_cast<std::uint8_t>(size>>(8*i));for(int i=0;i<8;++i)entry[16+i]=static_cast<std::uint8_t>(capacity>>(8*i));put32At(table,index*48+24,static_cast<std::uint32_t>(kinds[index]));put32At(table,index*48+28,index==0?1:3);const auto& regionHash=index==0?masterHash:emptyHash;std::copy(regionHash.begin(),regionHash.begin()+16,entry+32);offset+=capacity;}
  std::vector<std::uint8_t> headerHash=zcfSha256(header);std::copy(headerHash.begin(),headerHash.end(),header.begin()+32);std::ofstream outputFile(output,std::ios::binary);if(!outputFile){fail(error,"não foi possível criar ZCF");return false;}outputFile.write(reinterpret_cast<const char*>(header.data()),64);outputFile.write(reinterpret_cast<const char*>(table.data()),static_cast<std::streamsize>(table.size()));std::vector<std::uint8_t> padding(static_cast<std::size_t>(masterOffset-64-table.size()),0);outputFile.write(reinterpret_cast<const char*>(padding.data()),static_cast<std::streamsize>(padding.size()));outputFile.write(reinterpret_cast<const char*>(zip.data()),static_cast<std::streamsize>(zip.size()));std::vector<std::uint8_t> tail(static_cast<std::size_t>(totalCapacity-masterSize),0);outputFile.write(reinterpret_cast<const char*>(tail.data()),static_cast<std::streamsize>(tail.size()));outputFile.close();
  ZCardContainer verify(output);if(!verify.open(error))return false;if(report)*report=zipReport+"container=ZCF0\nregioes=MASTER,UPDATE,DLC,SAVE,USER\nregiao_MASTER_offset="+std::to_string(masterOffset)+"\nregiao_MASTER_bytes="+std::to_string(masterSize)+"\nverificacao=ok\n";return true;
}

bool ZCardBuilder::buildZuf(const std::filesystem::path& source,const std::filesystem::path& output,std::string* report,std::string* error){
  if(!std::filesystem::is_directory(source)){fail(error,"fonte ZUF não é um diretório: "+source.string());return false;}
  std::vector<std::pair<std::string,std::vector<std::uint8_t>>> files;
  try {
    for(const auto& item:std::filesystem::recursive_directory_iterator(source)) if(item.is_regular_file()){
      const auto rel=std::filesystem::relative(item.path(),source).generic_string();
      if(rel.empty()||rel.front()=='/'||rel.find("..")!=std::string::npos||rel.size()>65535){fail(error,"caminho inválido no pacote ZUF: "+rel);return false;}
      std::ifstream input(item.path(),std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)),{});
      files.emplace_back(rel,std::move(data));
    }
  } catch(const std::exception& ex){fail(error,std::string("falha ao ler fonte ZUF: ")+ex.what());return false;}
  std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  if(files.size()>std::numeric_limits<std::uint32_t>::max()){fail(error,"quantidade de arquivos ZUF excede o limite");return false;}
  std::uint64_t payloadSize=0;for(const auto& item:files)payloadSize+=4+8+item.first.size()+item.second.size();
  std::vector<std::uint8_t> header(32,0);header[0]='Z';header[1]='U';header[2]='F';header[3]='0';put16At(header,4,1);put16At(header,6,32);put32At(header,8,static_cast<std::uint32_t>(files.size()));put64At(header,12,payloadSize);put64At(header,20,0);
  std::ofstream outputFile(output,std::ios::binary|std::ios::trunc);if(!outputFile){fail(error,"não foi possível criar ZUF: "+output.string());return false;}
  outputFile.write(reinterpret_cast<const char*>(header.data()),static_cast<std::streamsize>(header.size()));
  for(const auto& item:files){std::vector<std::uint8_t> entry(12,0);put16At(entry,0,static_cast<std::uint16_t>(item.first.size()));put64At(entry,4,item.second.size());outputFile.write(reinterpret_cast<const char*>(entry.data()),static_cast<std::streamsize>(entry.size()));outputFile.write(item.first.data(),static_cast<std::streamsize>(item.first.size()));outputFile.write(reinterpret_cast<const char*>(item.second.data()),static_cast<std::streamsize>(item.second.size()));}
  outputFile.close();if(!outputFile){fail(error,"falha ao finalizar ZUF: "+output.string());return false;}
  if(report)*report="ZUF criado: "+output.string()+"\nversao=1\narquivos="+std::to_string(files.size())+"\nbytes_payload="+std::to_string(payloadSize)+"\nverificacao=ok\n";
  return true;
}
}
