#include "zconsole/asset_runtime.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;
namespace zconsole {
namespace {
std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> out; std::stringstream ss(value); std::string part;
  while (std::getline(ss, part, delimiter)) if (!part.empty()) out.push_back(part);
  return out;
}
std::string trim(std::string value) {
  const auto first=value.find_first_not_of(" \t\r\n"), last=value.find_last_not_of(" \t\r\n");
  return first==std::string::npos ? "" : value.substr(first,last-first+1);
}
}

AssetRuntime::AssetRuntime(fs::path zgfDirectory, std::size_t cacheBytes) : root_(std::move(zgfDirectory)), cacheLimit_(cacheBytes) {}

bool AssetRuntime::loadManifest(std::string* error) {
  std::ifstream input(root_ / "META" / "assets.zgf");
  if (!input) { if(error) *error="manifesto ausente: META/assets.zgf"; return false; }
  std::string line; std::size_t lineNo=0;
  while (std::getline(input,line)) {
    ++lineNo; line=trim(line); if(line.empty() || line[0]=='#' || line=="ZGF-0.1") continue;
    auto fields=split(line,'|');
    if(fields.empty()) continue;
    if(fields[0]=="asset" && fields.size()>=2) {
      AssetInfo item; item.path=fields[1];
      if(fields.size()>2) item.group=fields[2];
      if(fields.size()>3) item.priority=std::stoi(fields[3]);
      if(fields.size()>4 && fields[4]!="-") item.deadlineMs=std::stod(fields[4]);
      if(fields.size()>5 && fields[5]!="-") item.dependencies=split(fields[5],',');
      if(fields.size()>6 && fields[6]!="-") item.type=fields[6];
      assets_[item.path]=item;
    } else if(fields[0]=="scene" && fields.size()>=3) scenes_[fields[1]]=split(fields[2],',');
    else { if(error) *error="linha inválida no manifesto: "+std::to_string(lineNo); return false; }
  }
  return true;
}

std::vector<std::string> AssetRuntime::assetsForScene(const std::string& scene) const { auto it=scenes_.find(scene); return it==scenes_.end()?std::vector<std::string>{}:it->second; }
std::vector<std::string> AssetRuntime::assetsForGroup(const std::string& group) const { std::vector<std::string> out; for(const auto& [path,item]:assets_) if(item.group==group) out.push_back(path); return out; }
std::vector<AssetInfo> AssetRuntime::assetList() const { std::vector<AssetInfo> out; for(const auto& item:assets_) out.push_back(item.second); return out; }
std::vector<RequestInfo> AssetRuntime::requestList() const { std::vector<RequestInfo> out; for(const auto& item:requests_) out.push_back(item.second); return out; }

bool AssetRuntime::enqueue(const std::string& path,int priority,double deadline,std::vector<std::string>& visiting,std::string* error) {
  if(std::find(visiting.begin(),visiting.end(),path)!=visiting.end()) { if(error) *error="ciclo de dependências: "+path; return false; }
  if(!assets_.count(path)) { if(error) *error="asset não declarado: "+path; return false; }
  auto existing=requests_.find(path); if(existing!=requests_.end() && (existing->second.state==RequestState::Queued || existing->second.state==RequestState::Ready)) return true;
  visiting.push_back(path);
  for(const auto& dep:assets_.at(path).dependencies) if(!enqueue(dep,std::max(0,priority-10),deadline,visiting,error)) return false;
  visiting.pop_back();
  RequestInfo request; request.path=path; request.priority=priority; request.deadlineMs=deadline>=0?deadline:assets_.at(path).deadlineMs; requests_[path]=request; queue_.push_back(path); return true;
}

bool AssetRuntime::requestPrefetch(const std::vector<std::string>& paths,int priority,double deadline,std::string* error) {
  if(priority<0) { if(error) *error="prioridade inválida"; return false; }
  std::vector<std::string> visiting; for(const auto& path:paths) if(!enqueue(path,priority,deadline,visiting,error)) return false; return true;
}

std::vector<std::uint8_t> AssetRuntime::readFile(const std::string& path,std::string* error) const {
  std::ifstream file(root_/path,std::ios::binary); if(!file) { if(error) *error="arquivo ausente: "+path; return {}; }
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
}
std::vector<std::uint8_t> AssetRuntime::read(const std::string& path) {
  auto cached=cache_.find(path); if(cached!=cache_.end()) { ++metrics_.hits; return cached->second; }
  std::string error; auto data=readFile(path,&error); if(data.empty() && !fs::exists(root_/path)) { ++metrics_.misses; return {}; }
  ++metrics_.misses; metrics_.bytesRead+=data.size();
  while(cacheBytes_+data.size()>cacheLimit_ && !cache_.empty()) { cacheBytes_-=cache_.begin()->second.size(); cache_.erase(cache_.begin()); }
  if(data.size()<=cacheLimit_) { cacheBytes_+=data.size(); cache_[path]=data; } return data;
}

std::vector<std::string> AssetRuntime::processPrefetch(std::size_t maxRequests,std::size_t channels,double bandwidth,bool deadlineScheduler,double graceMs) {
  if(channels==0 || bandwidth<=0) return {};
  std::vector<std::string> processed; double elapsed=0.0;
  while(!queue_.empty() && processed.size()<maxRequests) {
    auto choose=queue_.begin();
    for(auto it=queue_.begin();it!=queue_.end();++it) {
      const auto& a=requests_.at(*it); const auto& b=requests_.at(*choose);
      bool aUrgent=a.deadlineMs>=0 && (b.deadlineMs<0 || a.deadlineMs<b.deadlineMs);
      if((deadlineScheduler && aUrgent) || (!deadlineScheduler && (a.priority<b.priority))) choose=it;
    }
    std::string path=*choose; queue_.erase(choose); auto& request=requests_.at(path); request.state=RequestState::Loading;
    auto data=read(path); if(data.empty() && !fs::exists(root_/path)) { request.state=RequestState::Failed; request.error="asset ausente"; ++metrics_.failed; processed.push_back(path); continue; }
    const auto channel=(processed.size()%channels)+1; request.channel=channel; request.state=RequestState::Ready; ++metrics_.processed;
    const double latency=(data.size()/bandwidth)*1000.0; elapsed+=latency; metrics_.latencyMs+=latency; auto& cm=metrics_.channels[channel]; ++cm.assets; cm.bytes+=data.size(); cm.latencyMs+=latency;
    if(request.deadlineMs<0) request.deadlineStatus=DeadlineStatus::Unscheduled; else if(elapsed<=request.deadlineMs) request.deadlineStatus=DeadlineStatus::OnTime; else if(elapsed<=request.deadlineMs+graceMs) request.deadlineStatus=DeadlineStatus::Late; else request.deadlineStatus=DeadlineStatus::Missed;
    processed.push_back(path);
  }
  return processed;
}

bool AssetRuntime::cancelPrefetch(const std::string& path) { auto it=requests_.find(path); if(it==requests_.end() || it->second.state!=RequestState::Queued) return false; it->second.state=RequestState::Cancelled; queue_.erase(std::remove(queue_.begin(),queue_.end(),path),queue_.end()); ++metrics_.cancelled; return true; }
std::optional<RequestInfo> AssetRuntime::status(const std::string& path) const { auto it=requests_.find(path); return it==requests_.end()?std::nullopt:std::optional<RequestInfo>(it->second); }
std::vector<std::uint8_t> AssetRuntime::stream(const std::string& path,std::size_t chunkBytes) { if(chunkBytes==0) return {}; auto data=read(path); metrics_.bytesStreamed+=data.size(); return data; }
const char* toString(RequestState state) { switch(state){case RequestState::Queued:return "queued";case RequestState::Loading:return "loading";case RequestState::Ready:return "ready";case RequestState::Failed:return "failed";case RequestState::Cancelled:return "cancelled";} return "unknown"; }
const char* toString(DeadlineStatus status) { switch(status){case DeadlineStatus::Unscheduled:return "unscheduled";case DeadlineStatus::OnTime:return "on_time";case DeadlineStatus::Late:return "late";case DeadlineStatus::Missed:return "missed";} return "unknown"; }
} // namespace zconsole
