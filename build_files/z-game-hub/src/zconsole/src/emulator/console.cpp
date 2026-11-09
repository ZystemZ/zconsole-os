#include "zconsole/emulator/console.hpp"
#include <cstring>
#include <algorithm>

namespace zconsole {
ZConsoleMemory::ZConsoleMemory(std::size_t sizeBytes):data_(sizeBytes,0){}
bool ZConsoleMemory::read(std::uint64_t address,void* buffer,std::size_t size) const { if(address>data_.size() || size>data_.size()-static_cast<std::size_t>(address)) return false; std::memcpy(buffer,data_.data()+address,size); return true; }
bool ZConsoleMemory::write(std::uint64_t address,const void* buffer,std::size_t size) { if(address>data_.size() || size>data_.size()-static_cast<std::size_t>(address)) return false; std::memcpy(data_.data()+address,buffer,size); return true; }
void ZConsoleMemory::clear(){std::fill(data_.begin(),data_.end(),0);}
void ZConsoleTiming::reset(){totalUs_=0;}
void ZConsoleTiming::advanceUs(std::uint64_t microseconds){totalUs_+=microseconds; frameTimeMs_=static_cast<double>(microseconds)/1000.0;}
ZConsole::ZConsole(std::size_t memoryBytes):memory_(memoryBytes),cpu_(memory_){}
void ZConsole::reset(){memory_.clear(); timing_.reset(); cpu_.reset(); running_=false;}
void ZConsole::tick(std::uint64_t microseconds){if(running_) timing_.advanceUs(microseconds);}
}
