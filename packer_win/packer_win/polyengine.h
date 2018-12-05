#ifndef POLYENGINE_H
#define POLYENGINE_H

#define ASMJIT_OS_WINDOWS
#define ASMJIT_STATIC
//#define DISABLE_OBFUSCATION
#include <asmjit.h>
#include <unordered_map>
#include <functional>
#include <deque>
#include <set>

using namespace asmjit::host;
using namespace asmjit;

class PolyEngine
{
public:
  PolyEngine();

  void setImageBase(ULONG_PTR imageBase) { m_imageBase = imageBase; }
  void assembleX86Loader(uint32_t secondStageAddress, uint8_t *secondStageBuffer, size_t secondStageSize, uint32_t virtualProtectIAT, std::vector<ULONG_PTR>& needReloc);
  void assembleX64Loader(uint64_t secondStageAddress, uint8_t *secondStageBuffer, size_t secondStageSize, uint64_t virtualProtectIAT, std::vector<ULONG_PTR>& needReloc);

  const uint8_t *getCode() const { return m_code.data(); }
  size_t getCodeSize() const { return m_code.size(); }

private:
  struct SOperation {
    uint32_t Op;
    uint32_t Operand;

    SOperation(uint32_t op, uint32_t operand) : Op(op), Operand(operand) {}
  };

  void load(const GpReg& dst, const Mem& src);
  void store(const Mem& dst, const GpReg& src);
  void move(const GpReg& dst, const GpReg& src);
  void move(const GpReg& dst, const Imm& src);
  void push(const GpReg& reg);
  void push(const Imm& value);
  void push(const Mem& value);
  void pop(const GpReg& reg);
  void pop(const Mem& value);
  void zeroTest(const GpReg& reg);
  void prologue(size_t& stackSpace);
  void epilogue(size_t stackSpace, const GpReg& destAddress);

  void testObfuscator();
  template<typename P1> void obfuscate(uint32_t code, P1 arg1)
  {
    m_asm.emit(code, arg1);
  }
  template<typename P1, typename P2> void obfuscate(uint32_t code, P1 arg1, P2 arg2)
  {
    if ((rand() % 100) < 20) {
#ifdef DISABLE_OBFUSCATION
      m_asm.emit(code, arg1, arg2);
#else
      static std::vector<GpReg> regs = { eax, ebx, ecx, edx, edi, esi };

      //temporary working register, will be restored after everything
      const GpReg& tmpReg = regs[rand() % regs.size()];
      Label dummy = m_asm.newLabel();
      Label dummy2 = m_asm.newLabel();

      //first variation: obfuscate with jmp or call
      bool jmpOrCall = (rand() % 100) > 50 ? true : false;

      if (jmpOrCall) {
        Label local = m_asm.newLabel();
        m_asm.push(tmpReg);
        m_asm.call(local);
        m_asm.bind(local);
        m_asm.pop(tmpReg);
        m_asm.jmp(dummy);
      }
      else
        m_asm.call(dummy);
      const size_t curOff = m_asm.getOffset();

      //second variation: encode a random multi-opcode instruction with random arguments
      //basically we try to encode something in the form:
      //mov eax, dword [xxxxxxxx]
      //add [xxxxxxxx], ebx
      //mov [eax], 0x11223344
      //...
      static std::vector<uint32_t> instrs = {
        kInstCmp,
        kInstAdd,
        kInstSub,
        kInstXor
      };
      uint32_t randAddr = rand_dword();
      uint32_t randValue = rand_dword();
      GpReg randReg = regs[rand() % regs.size()];
      uint32_t randInstr = instrs[rand() % instrs.size()];
      int randDispl = rand() % 128;
      uint32_t rnd = rand() % 5;
      switch (rnd) {
      case 0:
        m_asm.emit(randInstr, randReg, dword_ptr_abs(randAddr));
        break;

      case 1:
        m_asm.emit(randInstr, dword_ptr(randReg, randDispl), imm_u(randValue));
        break;

      case 2:
        m_asm.emit(randInstr, dword_ptr_abs(randAddr), randReg);
        break;

      case 3:
        m_asm.emit(randInstr, dword_ptr_abs(randAddr), imm_u(randValue));
        break;

      case 4:
        m_asm.emit(randInstr, randReg, imm_u(randValue));
        break;
      }
      const size_t offDispl = 3 + (rand() % 3);
      m_asm.setOffset(curOff + offDispl);
      if (jmpOrCall)
        m_asm.pop(tmpReg);
      m_asm.emit(code, arg1, arg2);
      m_asm.jmp(dummy2);
      m_asm.bind(dummy);
      if (jmpOrCall) {
        m_asm.add(tmpReg, 6+offDispl);
        m_asm.jmp(tmpReg);
      }
      else {
        m_asm.push(tmpReg);
        m_asm.mov(tmpReg, dword_ptr(esp, 4));
        m_asm.add(tmpReg, offDispl);
        m_asm.mov(dword_ptr(esp, 4), tmpReg);
        m_asm.pop(tmpReg);
        m_asm.ret();
      }
      m_asm.bind(dummy2);
#endif
    }
    else {
      m_asm.emit(code, arg1, arg2);
    }
  }

  GpReg randomRegister(bool set32bits = false);
  void releaseRegister(const GpReg& r);
  bool mustPreserve(const GpReg& r);

  void encryptSecondStageLoader(uint8_t *secondStageBuffer, size_t secondStageSize, const std::vector<SOperation>& ops);
  uint32_t applyOps(const std::vector<SOperation>& ops, uint32_t value);
  std::vector<SOperation> randomOps(size_t maxOps, bool useSwitch = false);
  void assembleOps(const std::vector<SOperation>& ops, GpReg& workReg);

protected:
  JitRuntime m_runtime;
  Assembler m_asm;
  std::vector<GpReg> m_regs;
  std::vector<GpReg> m_inuseRegs;
  std::vector<GpReg> m_presRegs;
  std::vector<uint8_t> m_code;
  ULONG_PTR m_imageBase;
};

#endif