#pragma once

#include <functional>
#include <type_traits>
#include <limits>
#include <array>
#include "rv_global.hpp"
#include "rv_memory.hpp"

constexpr uint32_t RV_PRIV_U = 0;
constexpr uint32_t RV_PRIV_S = 1;
constexpr uint32_t RV_PRIV_M = 3;

enum class riscv_register
{
    zero = 0,
    ra,
    sp,
    gp,
    tp,
    t0,
    t1, t2,
    s0,
    s1,
    a0, a1,
    a2, a3, a4, a5, a6, a7,
    s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
    t3, t4, t5, t6
};

class rv_cpu
{
public:
    rv_cpu() = delete;
    explicit rv_cpu(rv_memory& memory);

    void reset();
    void run(size_t nCycles);

    uint64_t cycle_count() const { return cycle_; }
    void update_mip(uint32_t irq_num, bool state);

private:
    uint32_t decode_rd(uint32_t insn) { return (insn >> 7) & 0x1F; }
    uint32_t decode_rs1(uint32_t insn) { return (insn >> 15) & 0x1F; }
    uint32_t decode_rs2(uint32_t insn) { return (insn >> 20) & 0x1F; }
    uint32_t decode_funct3(uint32_t insn) { return (insn >> 12) & 0b111; }

    // extract [low, high] bits from a 32bit value
    uint32_t bits(uint32_t val, uint32_t low, uint32_t high) { return (val >> low) & ((1 << (high - low + 1)) - 1); }

    // extract a single bit from a 32bit value
    uint32_t bit(uint32_t val, uint32_t bit) { return (val >> bit) & 1; }

    void next_insn(rv_uint cnt = 4) { pc_ += cnt; }
    void jump_insn(rv_uint newpc) { pc_ = newpc; }

    void raise_exception(rv_exception code);
    void raise_interrupt();

    void raise_illegal_instruction() { raise_exception(rv_exception::illegal_instruction); }
    void raise_memory_exception() { raise_exception(memory_.lastException()); }

    inline void execute_lui(uint32_t insn);
    inline void execute_auipc(uint32_t insn);
    inline void execute_jal(uint32_t insn);
    inline void execute_jalr(uint32_t insn);
    inline void execute_branch(uint32_t insn);
    inline void execute_load(uint32_t insn);
    inline void execute_store(uint32_t insn);
    inline void execute_amo(uint32_t insn);
    inline void execute_imm(uint32_t insn);
    inline void execute_op(uint32_t insn);
    inline void execute_misc_mem(uint32_t insn);
    inline void execute_system(uint32_t insn);

    bool csr_read(uint32_t csr, rv_uint& csr_value, bool write_back = false);
    bool csr_write(uint32_t csr, rv_uint csr_value);

    bool csr_rw(uint32_t csr, uint32_t rd, rv_uint new_value, uint32_t csrop);
    void execute_mret();

private:
    rv_uint pc_;
    std::array<rv_uint, 32> regs_;
    rv_uint amo_res_;
    rv_memory& memory_;

    bool exception_raised_;
    rv_exception exception_code_;

    // privilege mode is encoded in a secret, not accessible, register
    uint32_t priv_;

    // Counter/Timers
    uint64_t time_;
    uint64_t cycle_;
    uint64_t instret_;

    // Machine Trap Setup
    rv_uint mstatus_;
    rv_uint misa_;
    rv_uint mie_;
    rv_uint mtvec_;
    rv_uint mcounteren_;

    // Machine Trap Handling
    rv_uint mscratch_;
    rv_uint mepc_;
    rv_uint mcause_;
    rv_uint mvtval_;
    rv_uint mip_;
};