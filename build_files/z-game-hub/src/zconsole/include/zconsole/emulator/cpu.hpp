#pragma once
#include <cstdint>
#include <cstddef>
#include <set>
#include <string>

namespace zconsole {
class ZConsoleMemory;
struct ZCpuRegisters { std::uint32_t a=0,b=0,pc=0,sp=0; bool zero=false,halted=false; };
class ZVirtualCpu {
public:
  explicit ZVirtualCpu(ZConsoleMemory& memory):memory_(memory){}
  void reset();
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }
  bool paused() const { return paused_; }
  void addBreakpoint(std::uint32_t address) { breakpoints_.insert(address); }
  void removeBreakpoint(std::uint32_t address) { breakpoints_.erase(address); }
  void clearBreakpoints() { breakpoints_.clear(); }
  bool breakpointHit() const { return breakpointHit_; }
  bool step(std::string* error=nullptr);
  std::size_t run(std::size_t maxInstructions, std::string* error=nullptr);
  const ZCpuRegisters& registers() const { return regs_; }
  std::size_t instructions() const { return instructions_; }
private:
  bool read8(std::uint8_t& value,std::string* error);
  ZConsoleMemory& memory_; ZCpuRegisters regs_{}; std::size_t instructions_=0; bool paused_=false; bool breakpointHit_=false; std::set<std::uint32_t> breakpoints_;
};
}
