#include "polyengine.h"
#include "utils.h"
#include <stack>
#include <iterator>
#include <functional>
#include <algorithm>
#include <set>
#include <assert.h>

enum EOperations
{
  eO_Add,
  eO_Sub,
  eO_Xor,
  eO_Rol,
  eO_Ror,
  eO_SwitchRegister
};

#define SS_MAX_OPS 15

#ifndef _WIN64
#define FIRST_LOCAL 4
#define SECOND_LOCAL 8
#else
#define FIRST_LOCAL 8
#define SECOND_LOCAL 16
#endif


using namespace Utils;
PolyEngine::PolyEngine()
: m_runtime(), m_asm(&m_runtime)
{
  m_regs.emplace_back(zax);
  m_regs.emplace_back(zbx);
  m_regs.emplace_back(zcx);
  m_regs.emplace_back(zdx);
  m_regs.emplace_back(zdi);
  m_regs.emplace_back(zsi);
  //push x64 registers into the set
#ifdef _WIN64
  m_regs.emplace_back(r8);
  m_regs.emplace_back(r9);
  m_regs.emplace_back(r10);
  m_regs.emplace_back(r11);
  m_regs.emplace_back(r12);
  m_regs.emplace_back(r13);
  m_regs.emplace_back(r14);
  m_regs.emplace_back(r15);
#endif
}

GpReg PolyEngine::randomRegister(bool set32bits)
{
  GpReg r;
  do
  {
    int n = rand() % m_regs.size();
    r = m_regs[n];
  } while (std::find(m_inuseRegs.begin(), m_inuseRegs.end(), r) != m_inuseRegs.end());

  //store here each register that must be preserved, so we can assemble the function
  //prologue (and relative epilogue) when we're done
  if (mustPreserve(r)) {
    if (std::find(m_presRegs.begin(), m_presRegs.end(), r) == m_presRegs.end())
      m_presRegs.emplace_back(r);
  }

  m_inuseRegs.emplace_back(r);
  if (set32bits) {
    r.setSize(4);
    r.setType(kRegTypeGpd);
  }
  return r;
}

void PolyEngine::releaseRegister(const GpReg& r)
{
  if (std::find(m_inuseRegs.begin(), m_inuseRegs.end(), r) != m_inuseRegs.end())
    m_inuseRegs.erase(std::remove(m_inuseRegs.begin(), m_inuseRegs.end(), r), m_inuseRegs.end());
}

void PolyEngine::load(const GpReg& dst, const Mem& src)
{
  int n = rand() % 3;

  switch (n) {
  case 0:
    m_asm.mov(dst, src);
    break;

  case 1:
    m_asm.xor_(dst, dst);
    m_asm.add(dst, src);
    break;

  case 2:
    GpReg tmp = randomRegister();
    m_asm.push(tmp);
    m_asm.xor_(tmp, tmp);
    m_asm.mov(tmp, src);
    m_asm.mov(dst, tmp);
    m_asm.pop(tmp);
    releaseRegister(tmp);
    break;
  }
}

void PolyEngine::store(const Mem& dst, const GpReg& src)
{
  int n = rand() % 2;

  switch (n) {
  case 0:
    m_asm.mov(dst, src);
    break;

  case 1:
    m_asm.push(src);
    m_asm.pop(dst);
    break;
  }
}

void PolyEngine::move(const GpReg& dst, const GpReg& src)
{
  int n = rand() % 100;
  
  if (n < 50) {
    obfuscate(kInstMov, dst, src);
    //m_asm.mov(dst, src);
  }
  else if (n >= 50 && n < 75) {
    m_asm.push(src);
    m_asm.pop(dst);
  }
  else {
    GpReg tmp = randomRegister();
    m_asm.push(tmp);
    obfuscate(kInstXor, tmp, tmp);
    obfuscate(kInstMov, tmp, src);
    obfuscate(kInstXor, dst, dst);
    obfuscate(kInstAdd, dst, tmp);
    //m_asm.xor_(tmp, tmp);
    //m_asm.mov(tmp, src);
    //m_asm.xor_(dst, dst);
    //m_asm.add(dst, tmp);
    m_asm.pop(tmp);
    releaseRegister(tmp);
  }
}

void PolyEngine::move(const GpReg& dst, const Imm& src)
{
  int n = rand() % 100;

  if (n < 50) {
    //m_asm.mov(dst, src);
    obfuscate(kInstMov, dst, src);
  }
  else {
    m_asm.push(src);
    m_asm.pop(dst);
  }
}

void PolyEngine::push(const GpReg& reg)
{
  int n = rand() % 100;

  if (n < 50) {
    m_asm.push(reg);
  }
  else {
    obfuscate(kInstSub, esp, imm_u(4));
    obfuscate(kInstMov, dword_ptr(esp), reg);
    //m_asm.sub(esp, Imm(4));
    //m_asm.mov(dword_ptr(esp), reg);
  }
}

void PolyEngine::push(const Imm& value)
{
  m_asm.push(value);
}

void PolyEngine::push(const Mem& value)
{
  m_asm.push(value);
}

void PolyEngine::pop(const GpReg& reg)
{
  int r = (rand() % 100) + 1;

  if (r < 50)
    m_asm.pop(reg);
  else {
    m_asm.mov(reg, dword_ptr(esp));
    m_asm.add(esp, 4);
  }
}

void PolyEngine::zeroTest(const GpReg& reg)
{
  size_t r = (rand() % 100) + 1;
  if (r < 75)
    obfuscate(kInstTest, reg, reg);
  //m_asm.test(reg, reg);
  else
    obfuscate(kInstCmp, reg, 0);
    //m_asm.cmp(reg, 0);
}

bool PolyEngine::mustPreserve(const GpReg& r)
{
#ifndef _WIN64
  static const std::vector<GpReg> presRegs({ ebx, edi, esi });
#else
  static const std::vector<GpReg> presRegs({ rbx, rdi, rsi, r12, r13, r14, r15 });
#endif
  return std::find(presRegs.begin(), presRegs.end(), r) != presRegs.end();
}

void PolyEngine::prologue(size_t& stackSpace)
{
#ifndef _WIN64
  m_asm.push(ebp);
  m_asm.mov(ebp, esp);
#else
  /*
  //keep the stack aligned for x64 calling conventions
  if ((m_presRegs.size() % 2) != 0) {
    if((stackSpace & 0x8) == 0) {
      stackSpace += 0x8;
    }
  }
  else {
    if ((stackSpace & 0x8) != 0) {
      stackSpace += 0x8;
    }
  }*/
#endif
  for (auto i = m_presRegs.begin(), e = m_presRegs.end(); i != e; ++i) {
    m_asm.push(*i);
  }

  m_asm.sub(zsp, imm_u(stackSpace));
}

void PolyEngine::epilogue(size_t stackSpace, const GpReg& destAddress)
{
  m_asm.add(zsp, imm_u(stackSpace));
#ifndef _WIN64
  store(dword_ptr(esp, m_presRegs.size() * 4 + 4), destAddress);
  for (auto i = m_presRegs.rbegin(), e = m_presRegs.rend(); i != e; ++i) {
    pop(*i);
  }
  m_asm.mov(esp, ebp);
  m_asm.pop(ebp);
#else
  store(dword_ptr(rsp, (int32_t)(m_presRegs.size() * 8)), destAddress);
  for (auto i = m_presRegs.rbegin(), e = m_presRegs.rend(); i != e; ++i) {
    pop(*i);
  }
#endif
  m_asm.ret();
}

uint32_t PolyEngine::applyOps(const std::vector<SOperation>& ops, uint32_t value)
{
  uint32_t res = value;
  for (auto i = ops.begin(), e = ops.end(); i != e; ++i) {
    const SOperation& op = *i;
    switch (op.Op) {
    case eO_Add:
      res += op.Operand;
      break;

    case eO_Sub:
      res -= op.Operand;
      break;

    case eO_Xor:
      res ^= op.Operand;
      break;

    case eO_Rol:
      res = _rotl(res, op.Operand);
      break;

    case eO_Ror:
      res = _rotr(res, op.Operand);
      break;
    }
  }
  return res;
}

std::vector<PolyEngine::SOperation> PolyEngine::randomOps(size_t maxOps, bool useSwitch)
{
  int numOps = (rand() % maxOps) + 1;
  const int optypes = useSwitch ? 6 : 5;
  std::vector<PolyEngine::SOperation> ops;
  ops.reserve(numOps);

  for (int i = 0; i < numOps; ++i) {
    uint32_t operation = rand() % optypes;
    uint32_t operand = rand_dword();
    switch (operation) {
    case eO_Rol:
    case eO_Ror:
      operand = (rand() % 16) + 1;
      break;
    }
    ops.emplace_back(operation, operand);
  }

  return ops;
}

void PolyEngine::assembleOps(const std::vector<SOperation>& ops, GpReg& workReg)
{
  for (auto i = ops.rbegin(), e = ops.rend(); i != e; ++i) {
    const SOperation& op = *i;
    switch (op.Op) {
    case eO_Add:
      obfuscate(kInstSub, workReg, imm_u(op.Operand));
      //m_asm.sub(workReg, imm_u(op.Operand));
      break;

    case eO_Sub:
      obfuscate(kInstAdd, workReg, imm_u(op.Operand));
      //m_asm.add(workReg, imm_u(op.Operand));
      break;

    case eO_Xor:
      obfuscate(kInstXor, workReg, imm_u(op.Operand));
      //m_asm.xor_(workReg, imm_u(op.Operand));
      break;

    case eO_Rol:
      obfuscate(kInstRor, workReg, imm_u(op.Operand));
      //m_asm.ror(workReg, imm_u(op.Operand));
      break;

    case eO_Ror:
      obfuscate(kInstRol, workReg, imm_u(op.Operand));
      //m_asm.rol(workReg, imm_u(op.Operand));
      break;

    case eO_SwitchRegister:
      GpReg tmp = randomRegister();
      move(tmp, workReg);
      releaseRegister(workReg);
      workReg = tmp;
      break;
    }
  }
}

void PolyEngine::encryptSecondStageLoader(uint8_t *secondStageBuffer, size_t secondStageSize, const std::vector<SOperation>& ops)
{
  size_t ndwords = secondStageSize / 4;
  uint32_t *pSStage = reinterpret_cast<uint32_t*>(secondStageBuffer);

  for (size_t i = 0; i < ndwords; ++i) {
    uint32_t curdword = *pSStage;
    *pSStage++ = applyOps(ops, curdword);
  }
}

void PolyEngine::testObfuscator()
{
  obfuscate(kInstPush, ebx);
  obfuscate(kInstPush, eax);
  obfuscate(kInstMov, eax, 45);
  obfuscate(kInstAdd, eax, 45);
  obfuscate(kInstMov, ebx, eax);
  obfuscate(kInstPop, eax);
  obfuscate(kInstPop, ebx);

  m_code.resize(m_asm.getCodeSize());
  std::copy(m_asm.getBuffer(), m_asm.getBuffer() + m_asm.getCodeSize(), m_code.begin());

}
//this will assemble the first stage loader and encrypt the second stage one
//encryption is performed with a random number of operations over 4 bytes at a time
void PolyEngine::assembleX86Loader(uint32_t secondStageAddress, uint8_t *secondStageBuffer, size_t secondStageSize, uint32_t virtualProtectIAT, std::vector<ULONG_PTR>& needReloc)
{
  assert(secondStageSize % 4 == 0);
  int numOps = (rand() % SS_MAX_OPS) + 1;
  std::vector<SOperation> ops = randomOps(SS_MAX_OPS, true);

  uint32_t workAddress = applyOps(ops, secondStageAddress);
  
  //begin to assemble loader
  //choose a working register
  GpReg workReg = randomRegister();

  move(workReg, imm_u(workAddress));
  assembleOps(ops, workReg);
  ops.clear();

  ops = randomOps(SS_MAX_OPS, false);
  //encrypt the original loader
  uint32_t *pSStage = reinterpret_cast<uint32_t*>(secondStageBuffer);
  size_t ndwords = secondStageSize / 4;
  for (size_t i = 0; i < ndwords; ++i) {
    uint32_t curDword = applyOps(ops, *pSStage);
    *pSStage++ = curDword;
  }

  obfuscate(kInstAdd, workReg, imm_u(m_imageBase));//m_asm.add(workReg, imm_u(m_imageBase));
  needReloc.push_back(m_asm.getOffset() - sizeof(ULONG_PTR));
  //generate code to invoke VirtualProtect and decrypt the second stage loader
  Label loaderEnd = m_asm.newLabel();
  Label loopStart = m_asm.newLabel();
  Label loopEnd = m_asm.newLabel();
  /*
  GpReg r = randomRegister();
  if (!mustPreserve(workReg)) {
    push(workReg);
  }

  const uint32_t xorvalue = rand_dword();
  const uint32_t newValue = (virtualProtectIAT - (uint32_t)m_imageBase) ^ xorvalue;
  move(r, imm_u(newValue));

  m_asm.xor_(r, imm_u(xorvalue));
  m_asm.add(r, imm_u(m_imageBase));
  needReloc.push_back(m_asm.getOffset() - sizeof(ULONG_PTR));

  
  GpReg r2 = randomRegister();
  load(r2, dword_ptr(r));
  store(dword_ptr(ebp, -8), r2);
  releaseRegister(r2);
  m_asm.lea(r, dword_ptr(ebp, -4));
  push(r); //will be used to store the previous protection
  push(imm_u(0x40));
  push(imm_u(secondStageSize));
  push(workReg);
  //emit a call dowrd ptr [virtualProtectIAT]
  m_asm.call(dword_ptr(ebp, -8));
  m_asm.test(eax, eax);
  m_asm.jz(loaderEnd);
  if (!mustPreserve(workReg)) {
    pop(workReg);
  }
  releaseRegister(r);
  */
  push(workReg);

  GpReg r = randomRegister();
  move(r, imm_u(ndwords));
  m_asm.bind(loopStart);
  zeroTest(r);
  m_asm.jz(loopEnd);
  GpReg curDword = randomRegister(true);
  load(curDword, dword_ptr(workReg));
  //and generate the random decryption routine
  assembleOps(ops, curDword);
  store(dword_ptr(workReg), curDword);
  obfuscate(kInstAdd, workReg, 4);
  //m_asm.add(workReg, 4);
  m_asm.dec(r);
  m_asm.jmp(loopStart);
  m_asm.bind(loopEnd);
  pop(workReg);
  /*
  if (!mustPreserve(workReg)) {
    push(workReg);
  }
  //restore protection
  m_asm.lea(r, dword_ptr(ebp, -4));
  push(r);
  push(dword_ptr(ebp, -4));
  push(imm_u(secondStageSize));
  push(workReg);
  //emit a call dowrd ptr [virtualProtectIAT]
  m_asm.call(dword_ptr(ebp, -8));
  if (!mustPreserve(workReg)) {
    pop(workReg);
  }
  */
  m_asm.bind(loaderEnd);

  size_t sspace = 8 + (rand() % 5) * 4;
  epilogue(sspace, workReg);

  m_code.resize(m_asm.getCodeSize());
  std::copy(m_asm.getBuffer(), m_asm.getBuffer() + m_asm.getCodeSize(), m_code.begin());
  m_asm.reset();
  prologue(sspace);
  m_code.insert(m_code.begin(), m_asm.getBuffer(), m_asm.getBuffer() + m_asm.getCodeSize());
  std::transform(needReloc.begin(), needReloc.end(), needReloc.begin(), [&](ULONG_PTR v) { return v + m_asm.getCodeSize(); });
}

void PolyEngine::assembleX64Loader(uint64_t secondStageAddress, uint8_t *secondStageBuffer, size_t secondStageSize, uint64_t virtualProtectIAT, std::vector<ULONG_PTR>& needReloc)
{
  /*
  assert(secondStageSize % 4 == 0);
  int numOps = (rand() % SS_MAX_OPS) + 1;
  std::vector<SOperation> ops = randomOps(SS_MAX_OPS, true);

  uint64_t workAddress = applyOps(ops, secondStageAddress);

  //begin to assemble loader
  //choose a working register
  GpReg workReg = randomRegister();

  move(workReg, imm_u(workAddress));
  assembleOps(ops, workReg);
  ops.clear();

  ops = randomOps(SS_MAX_OPS, false);
  //encrypt the original loader
  uint32_t *pSStage = reinterpret_cast<uint32_t*>(secondStageBuffer);
  size_t ndwords = secondStageSize / 4;
  for (size_t i = 0; i < ndwords; ++i) {
    uint32_t curDword = applyOps(ops, *pSStage);
    *pSStage++ = curDword;
  }

  {
    GpReg r = randomRegister();
    m_asm.mov(r, imm_u(m_imageBase));
    needReloc.push_back(m_asm.getOffset() - sizeof(ULONG_PTR));
    m_asm.add(workReg, r);
    releaseRegister(r);
  }

  //generate code to invoke VirtualProtect and decrypt the second stage loader
  Label loaderEnd = m_asm.newLabel();
  Label loopStart = m_asm.newLabel();
  Label loopEnd = m_asm.newLabel();
  GpReg r = randomRegister();
  if (!mustPreserve(workReg)) {
    push(workReg);
  }

  const uint64_t xorvalue = rand_qword();
  const uint64_t newValue = (virtualProtectIAT - m_imageBase) ^ xorvalue;
  move(r, imm_u(newValue));

  {
    GpReg tmp = randomRegister();
    move(tmp, imm_u(xorvalue));
    m_asm.xor_(r, tmp);
    move(tmp, imm_u(m_imageBase));
    needReloc.push_back(m_asm.getOffset() - sizeof(ULONG_PTR));
    m_asm.add(r, tmp);
    releaseRegister(tmp);
  }

  GpReg r2 = randomRegister();
  load(r2, dword_ptr(r));
  store(dword_ptr(ebp, -8), r2);
  releaseRegister(r2);
  m_asm.lea(r, dword_ptr(ebp, -4));
  push(r); //will be used to store the previous protection
  push(imm_u(0x40));
  push(imm_u(secondStageSize));
  push(workReg);
  //emit a call dowrd ptr [virtualProtectIAT]
  m_asm.call(dword_ptr(ebp, -8));
  m_asm.test(eax, eax);
  m_asm.jz(loaderEnd);
  if (!mustPreserve(workReg)) {
    pop(workReg);
  }
  releaseRegister(r);
  
  push(workReg);

  r = randomRegister();
  move(r, imm_u(ndwords));
  m_asm.bind(loopStart);
  zeroTest(r);
  m_asm.jz(loopEnd);
  GpReg curDword = randomRegister(true);
  load(curDword, dword_ptr(workReg));
  //and generate the random decryption routine
  assembleOps(ops, curDword);
  store(dword_ptr(workReg), curDword);
  m_asm.add(workReg, 4);
  m_asm.dec(r);
  m_asm.jmp(loopStart);
  m_asm.bind(loopEnd);
  pop(workReg);

  if (!mustPreserve(workReg)) {
    push(workReg);
  }
  //restore protection
  m_asm.lea(r, dword_ptr(ebp, -4));
  push(r);
  push(dword_ptr(ebp, -4));
  push(imm_u(secondStageSize));
  push(workReg);
  //emit a call dowrd ptr [virtualProtectIAT]
  m_asm.call(dword_ptr(ebp, -8));
  if (!mustPreserve(workReg)) {
    pop(workReg);
  }

  m_asm.bind(loaderEnd);

  size_t sspace = 4*8 + (rand() % 5) * 4;
  epilogue(sspace, workReg);

  m_code.resize(m_asm.getCodeSize());
  std::copy(m_asm.getBuffer(), m_asm.getBuffer() + m_asm.getCodeSize(), m_code.begin());
  m_asm.reset();
  prologue(sspace);
  m_code.insert(m_code.begin(), m_asm.getBuffer(), m_asm.getBuffer() + m_asm.getCodeSize());
  std::transform(needReloc.begin(), needReloc.end(), needReloc.begin(), [&](ULONG_PTR v) { return v + m_asm.getCodeSize(); });*/
}