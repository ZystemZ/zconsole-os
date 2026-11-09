#include "zconsole/zcard/virtual_card.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
namespace zconsole {
VirtualZCard::VirtualZCard(std::filesystem::path root):root_(std::move(root)),runtime_(root_){ }
bool VirtualZCard::mount(std::string* error){
  if(std::filesystem::is_regular_file(root_)){
    std::ifstream input(root_,std::ios::binary);char magic[4]{};input.read(magic,4);
    if(input.gcount()==4&&magic[0]=='Z'&&magic[1]=='C'&&magic[2]=='F'&&magic[3]=='0'){
      container_=std::make_unique<ZCardContainer>(root_);if(!container_->open(error)){container_.reset();return false;}
    } else if(input.gcount()==4&&magic[0]=='P'&&magic[1]=='K'){
      archive_=std::make_unique<ZCardArchive>(root_);if(!archive_->open(error)){archive_.reset();return false;}
    } else {if(error)*error="cartão inválido: magic ZCF0 ou ZIP ausente";return false;}
    mounted_=true;return true;
  }
  if(!std::filesystem::is_directory(root_)){if(error)*error="Virtual ZCARD não é diretório nem arquivo: "+root_.string();return false;}
  const auto meta=root_/"META";
  const bool hasAssetManifest=std::filesystem::is_regular_file(meta/"assets.zgf");
  const bool hasGameManifest=std::filesystem::is_regular_file(meta/"zgame.json");
  const bool hasRomManifest=std::filesystem::is_regular_file(meta/"rom-runtime.json");
  if(hasAssetManifest){if(!runtime_.loadManifest(error))return false;}
  else if(!hasGameManifest&&!hasRomManifest){if(error)*error="Virtual ZCARD inválido: esperado META/assets.zgf, META/zgame.json ou META/rom-runtime.json";return false;}
  mounted_=true;
  return true;
}
bool VirtualZCard::verify(std::string* error){if(!mounted_&&!mount(error))return false;if(container_)return container_->verify(error);return true;}
std::unique_ptr<ZAssetStream> VirtualZCard::openStream(const std::string& asset,std::string* error) const{
  if(!mounted_){if(error)*error="Virtual ZCARD não montada";return nullptr;}
  if(container_)return container_->openStream(asset,error);
  if(archive_)return archive_->openStream(asset,error);
  auto stream=createFileAssetStream(root_.string());if(!stream->open(asset,error))return nullptr;return stream;
}
std::vector<std::string> VirtualZCard::entries(std::string* error) const{
  if(!mounted_){if(error)*error="Virtual ZCARD não montada";return{};}
  if(container_)return container_->entries();
  if(archive_)return archive_->entries();
  std::vector<std::string> result;try{for(const auto& item:std::filesystem::recursive_directory_iterator(root_))if(item.is_regular_file())result.push_back(std::filesystem::relative(item.path(),root_).generic_string());}catch(const std::exception& ex){if(error)*error=std::string("falha ao enumerar cartão: ")+ex.what();return{};}std::sort(result.begin(),result.end());return result;
}
bool VirtualZCard::writeStream(ZcfRegionKind region,const std::string& asset,const std::vector<std::uint8_t>& data,std::string* error){
  if(!mounted_&&!mount(error))return false;
  if(!container_){if(error)*error="escrita de região requer container ZCF";return false;}
  return container_->writeStream(region,asset,data,error);
}
}
