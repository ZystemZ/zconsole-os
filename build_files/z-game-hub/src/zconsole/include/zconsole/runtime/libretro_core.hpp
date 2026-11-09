#pragma once
#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace zconsole {

class LibretroCore {
public:
  LibretroCore() = default;
  ~LibretroCore();
  LibretroCore(const LibretroCore&) = delete;
  LibretroCore& operator=(const LibretroCore&) = delete;

  bool open(const std::filesystem::path& library, std::string* error = nullptr);
  bool loadRom(const std::filesystem::path& rom, std::string* error = nullptr);
  bool runFrame(std::string* error = nullptr);
  void unload();
  void close();
  bool loaded() const { return loaded_; }
  const std::vector<std::uint32_t>& videoFrame() const { return videoFrame_; }
  std::uint32_t videoWidth() const { return videoWidth_; }
  std::uint32_t videoHeight() const { return videoHeight_; }
  std::uint32_t audioSampleRate() const { return audioSampleRate_; }
  std::uint64_t frames() const { return frames_; }
  void setButtonState(unsigned retroButton, bool pressed);
  void clearButtons();

private:
  using Handle = void*;
  Handle library_ = nullptr;
  bool initialized_ = false;
  bool loaded_ = false;
  std::uint32_t videoWidth_ = 0;
  std::uint32_t videoHeight_ = 0;
  std::uint32_t audioSampleRate_ = 48000;
  std::uint64_t frames_ = 0;
  std::vector<std::uint32_t> videoFrame_;
  std::array<bool, 16> buttons_{};
  std::string systemDirectory_;
  std::string saveDirectory_;
  std::filesystem::path loadedRom_;

  bool initialize(std::string* error);
  void receiveVideo(const void* data, unsigned width, unsigned height, std::size_t pitch);
  std::size_t receiveAudio(const std::int16_t* data, std::size_t frames);
  bool environment(unsigned command, void* data);
  static bool environmentCallback(unsigned command, void* data);
  static void videoCallback(const void* data, unsigned width, unsigned height, std::size_t pitch);
  static std::size_t audioCallback(const std::int16_t* data, std::size_t frames);
  static void audioSampleCallback(std::int16_t, std::int16_t);
  static void inputPollCallback();
  static std::int16_t inputStateCallback(unsigned, unsigned, unsigned, unsigned);
  static LibretroCore* active_;
};

} // namespace zconsole
