#pragma once
#include <cstdint>
#include <deque>
#include <vector>
namespace zconsole {
struct ZColor { std::uint8_t r=0,g=0,b=0,a=255; };
class ZConsoleGraphics { public: ZConsoleGraphics(std::uint32_t width=320,std::uint32_t height=180); void clear(ZColor color); void fillRect(std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t h,ZColor color); const std::vector<std::uint8_t>& framebuffer() const{return pixels_;} std::uint32_t width()const{return width_;}std::uint32_t height()const{return height_;} private:std::uint32_t width_,height_;std::vector<std::uint8_t> pixels_; };
enum class ZInputKey { Up,Down,Left,Right,Action,Start };
struct ZInputEvent { ZInputKey key; bool pressed; };
class ZConsoleInput { public: void push(ZInputEvent event); bool poll(ZInputEvent& event); bool isDown(ZInputKey key)const; private:std::deque<ZInputEvent> events_;std::vector<ZInputKey> down_; };
struct ZTone { std::uint32_t frequency=0,durationMs=0; };
class ZConsoleAudio { public: void playTone(std::uint32_t frequency,std::uint32_t durationMs); const std::vector<ZTone>& tones()const{return tones_;} void clear(){tones_.clear();} private:std::vector<ZTone> tones_; };
}
