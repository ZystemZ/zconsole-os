#include "zconsole/emulator/cpu.hpp"
#include "zconsole/emulator/console.hpp"
namespace zconsole {
namespace {
bool readImm(ZConsoleMemory& memory, std::uint32_t address, std::uint32_t& value) { return memory.read(address, &value, 4); }
}
void ZVirtualCpu::reset(){regs_={}; regs_.sp=static_cast<std::uint32_t>(memory_.size()); instructions_=0; paused_=false; breakpointHit_=false;}
bool ZVirtualCpu::read8(std::uint8_t& value,std::string* error){if(!memory_.read(regs_.pc,&value,1)){if(error)*error="CPU PC fora da memória";regs_.halted=true;return false;}++regs_.pc;return true;}
bool ZVirtualCpu::step(std::string* error){
  if(regs_.halted||paused_)return false;
  std::uint8_t op=0; if(!read8(op,error))return false; std::uint32_t imm=0;
  switch(op){
    case 0x00: break;
    case 0x01: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="LOAD_IMM truncado";return false;} regs_.pc+=4;regs_.a=imm;regs_.zero=regs_.a==0;break;
    case 0x02: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="ADD_IMM truncado";return false;} regs_.pc+=4;regs_.a+=imm;regs_.zero=regs_.a==0;break;
    case 0x03: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="STORE truncado";return false;} regs_.pc+=4;if(!memory_.write(imm,&regs_.a,4)){if(error)*error="STORE fora da memória";return false;}break;
    case 0x04: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="JMP truncado";return false;}regs_.pc=imm;break;
    case 0x05: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="CMP_IMM truncado";return false;}regs_.pc+=4;regs_.zero=regs_.a==imm;break;
    case 0x06: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="JZ truncado";return false;}regs_.pc+=4;if(regs_.zero)regs_.pc=imm;break;
    case 0x07: if(regs_.sp<4||!memory_.write(regs_.sp-4,&regs_.a,4)){if(error)*error="PUSH_A sem espaço na pilha";return false;}regs_.sp-=4;break;
    case 0x08: if(regs_.sp>memory_.size()-4||!memory_.read(regs_.sp,&regs_.a,4)){if(error)*error="POP_A com pilha vazia";return false;}regs_.sp+=4;regs_.zero=regs_.a==0;break;
    case 0x09: if(!readImm(memory_,regs_.pc,imm)){if(error)*error="CALL truncado";return false;}regs_.pc+=4;if(regs_.sp<4||!memory_.write(regs_.sp-4,&regs_.pc,4)){if(error)*error="CALL sem espaço na pilha";return false;}regs_.sp-=4;regs_.pc=imm;break;
    case 0x0a: if(regs_.sp>memory_.size()-4||!memory_.read(regs_.sp,&imm,4)){if(error)*error="RET com pilha vazia";return false;}regs_.sp+=4;regs_.pc=imm;break;
    case 0xff: regs_.halted=true;break;
    default: if(error)*error="opcode desconhecido";regs_.halted=true;return false;
  }
  ++instructions_; return true;
}
std::size_t ZVirtualCpu::run(std::size_t maxInstructions,std::string* error){std::size_t count=0;breakpointHit_=false;while(count<maxInstructions&&!regs_.halted&&!paused_){if(breakpoints_.count(regs_.pc)){breakpointHit_=true;paused_=true;break;}if(!step(error))break;++count;}return count;}
}
