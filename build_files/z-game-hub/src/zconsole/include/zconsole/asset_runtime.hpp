#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zconsole {

enum class RequestState { Queued, Loading, Ready, Failed, Cancelled };
enum class DeadlineStatus { Unscheduled, OnTime, Late, Missed };

struct AssetInfo {
  std::string path;
  std::string type = "binary";
  std::string group = "streaming";
  int priority = 100;
  double deadlineMs = -1.0;
  std::vector<std::string> dependencies;
};

struct RequestInfo {
  std::string path;
  int priority = 100;
  RequestState state = RequestState::Queued;
  DeadlineStatus deadlineStatus = DeadlineStatus::Unscheduled;
  double deadlineMs = -1.0;
  std::size_t channel = 0;
  std::string error;
};

struct ChannelMetrics { std::size_t assets = 0; std::uintmax_t bytes = 0; double latencyMs = 0.0; };
struct RuntimeMetrics { std::size_t hits=0, misses=0, processed=0, failed=0, cancelled=0; std::uintmax_t bytesRead=0, bytesStreamed=0; double latencyMs=0.0; std::map<std::size_t, ChannelMetrics> channels; };

class AssetRuntime {
public:
  explicit AssetRuntime(std::filesystem::path zgfDirectory, std::size_t cacheBytes = 8 * 1024 * 1024);
  bool loadManifest(std::string* error = nullptr);
  std::vector<std::string> assetsForScene(const std::string& scene) const;
  std::vector<std::string> assetsForGroup(const std::string& group) const;
  bool requestPrefetch(const std::vector<std::string>& paths, int priority=100, double deadlineMs=-1.0, std::string* error=nullptr);
  std::vector<std::string> processPrefetch(std::size_t maxRequests=SIZE_MAX, std::size_t channels=1, double bandwidthBytesPerSec=100*1024*1024, bool deadlineScheduler=false, double graceMs=0.0);
  bool cancelPrefetch(const std::string& path);
  std::optional<RequestInfo> status(const std::string& path) const;
  std::vector<std::uint8_t> read(const std::string& path);
  std::vector<std::uint8_t> stream(const std::string& path, std::size_t chunkBytes=64*1024);
  const RuntimeMetrics& metrics() const { return metrics_; }
  std::vector<AssetInfo> assetList() const;
  std::vector<RequestInfo> requestList() const;

private:
  bool enqueue(const std::string& path, int priority, double deadlineMs, std::vector<std::string>& visiting, std::string* error);
  std::vector<std::uint8_t> readFile(const std::string& path, std::string* error) const;
  std::filesystem::path root_;
  std::size_t cacheLimit_;
  std::map<std::string, AssetInfo> assets_;
  std::map<std::string, std::vector<std::string>> scenes_;
  std::map<std::string, RequestInfo> requests_;
  std::vector<std::string> queue_;
  std::map<std::string, std::vector<std::uint8_t>> cache_;
  std::size_t cacheBytes_ = 0;
  RuntimeMetrics metrics_;
};

const char* toString(RequestState state);
const char* toString(DeadlineStatus status);

} // namespace zconsole
