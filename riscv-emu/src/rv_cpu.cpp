#include <memory.h>
#include <limits>
#include "rv_cpu.hpp"
#include "rv_exceptions.hpp"
#include "rv_memory.hpp"
#include "rv_bits.hpp"

constexpr uint32_t kRiscvOpcodeMask = 0x7F;

#define RV_MSTATUS_UIE_SHIFT 0
#define RV_MSTATUS_SIE_SHIFT 1
#define RV_MSTATUS_MIE_SHIFT 3
#define RV_MSTATUS_UPIE_SHIFT 4
#define RV_MSTATUS_SPIE_SHIFT 5
#define RV_MSTATUS_MPIE_SHIFT 7
#define RV_MSTATUS_SPP_SHIFT 8
#define RV_MSTATUS_MPP_SHIFT 11

#define RV_MSTATUS_UIE  (1 << RV_MSTATUS_UIE_SHIFT)
#define RV_MSTATUS_SIE  (1 << RV_MSTATUS_SIE_SHIFT)
#define RV_MSTATUS_MIE  (1 << RV_MSTATUS_MIE_SHIFT)
#define RV_MSTATUS_UPIE (1 << RV_MSTATUS_UPIE_SHIFT)
#define RV_MSTATUS_SPIE (1 << RV_MSTATUS_SPIE_SHIFT)
#define RV_MSTATUS_MPIE (1 << RV_MSTATUS_MPIE_SHIFT)
#define RV_MSTATUS_SPP  (1 << RV_MSTATUS_SPP_SHIFT)
#define RV_MSTATUS_MPP  (3 << RV_MSTATUS_MPP_SHIFT)

constexpr auto RV_MIP_USIP = rv_bitfield<1,0>{};
constexpr auto RV_MIP_SSIP = rv_bitfield<1,1>{};
constexpr auto RV_MIP_MSIP = rv_bitfield<1,3>{};
constexpr auto RV_MIP_UTIP = rv_bitfield<1,4>{};
constexpr auto RV_MIP_STIP = rv_bitfield<1,5>{};
constexpr auto RV_MIP_MTIP = rv_bitfield<1,7>{};
constexpr auto RV_MIP_UEIP = rv_bitfield<1,8>{};
constexpr auto RV_MIP_SEIP = rv_bitfield<1,9>{};
constexpr auto RV_MIP_MEIP = rv_bitfield<1,11>{};

constexpr auto RV_MIE_USIE = rv_bitfield<1,0>{};
constexpr auto RV_MIE_SSIE = rv_bitfield<1,1>{};
constexpr auto RV_MIE_MSIE = rv_bitfield<1,3>{};
constexpr auto RV_MIE_UTIE = rv_bitfield<1,4>{};
constexpr auto RV_MIE_STIE = rv_bitfield<1,5>{};
constexpr auto RV_MIE_MTIE = rv_bitfield<1,7>{};
constexpr auto RV_MIE_UEIE = rv_bitfield<1,8>{};
constexpr auto RV_MIE_SEIE = rv_bitfield<1,9>{};
constexpr auto RV_MIE_MEIE = rv_bitfield<1,11>{};

constexpr auto RV_MCOUNTEREN_CY = rv_bitfield<1,0>{};
constexpr auto RV_MCOUNTEREN_TM = rv_bitfield<1,1>{};
constexpr auto RV_MCOUNTEREN_IR = rv_bitfield<1,2>{};

enum class rv_opcode: uint32_t
{
    lui = 0b01101,
    auipc = 0b00101,
    jal = 0b11011,
    jalr = 0b11001,
    branch = 0b11000,
    load = 0b00000,
    store = 0b01000,
    imm = 0b00100,
    op = 0b01100,
    misc_mem = 0b00011,
    system  = 0b11100,
    amo = 0b01011
};

enum class rv_csr: uint32_t
{
    cycle = 0xC00,
    time = 0xC01,
    instret = 0xC02,
    cycleh = 0xC80,
    timeh = 0xC81,
    instreth = 0xC82,

    mvendorid = 0xF11,
    marchid = 0xF12,
    mimpid = 0xF13,
    mhartid = 0xF14,

    mstatus = 0x300,
    misa = 0x301,
    mie = 0x304,
    mtvec = 0x305,
    mcounteren = 0x306,

    mscratch = 0x340,
    mepc = 0x341,
    mcause = 0x342,
    mtval = 0x343,
    mip = 0x344
};

inline const char *const g_regNames[] = {
    "zero",
    "ra",
    "sp",
    "gp",
    "tp",
    "t0",
    "t1", "t2",
    "s0",
    "s1",
    "a0", "a1",
    "a2", "a3", "a4", "a5", "a6", "a7",
    "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
    "t3", "t4", "t5", "t6"
};

rv_cpu::rv_cpu(rv_memory& memory)
        : memory_{memory}
{

}

void rv_cpu::reset()
{
    // execution starts at 0x1000 in machine mode
    pc_ = 0x1000;

    // we run in machine mode at boot
    priv_ = RV_PRIV_M;

    // default exception vector at 0x0, fetching from here will cause an exception
    mtvec_ = 0x0000;
    mcause_ = 0;
    mvtval_ = 0;

    regs_.fill(0x66666666);
    regs_[0] = 0;

    cycle_ = 0;
    instret_ = 0;

    mip_ = 0;
    mie_ = 0;

    exception_raised_ = false;
}

void rv_cpu::run(size_t nCycles)
{
    auto c = nCycles;

    const auto pending_irqs = mie_ & mip_;
    if (pending_irqs != 0) {
        const auto irq_num = __builtin_ctz(pending_irqs);
        exception_code_ = (rv_exception)(0x80000000 | irq_num);
        exception_raised_ = true;
    }

    c += 1;
    while(likely(!exception_raised_)) {
        if (unlikely(!--c))
            break;

        uint32_t insn;
        if (unlikely(!memory_.read(pc_, insn))) {
            raise_memory_exception();
            break;
        }

        // add support for compressed instructions!
        const auto opcode = (rv_opcode) ((insn & kRiscvOpcodeMask) >> 2);
        switch (opcode) {
        case rv_opcode::load:
            execute_load(insn);
            break;
        case rv_opcode::misc_mem:
            execute_misc_mem(insn);
            break;
        case rv_opcode::imm:
            execute_imm(insn);
            break;
        case rv_opcode::auipc:
            execute_auipc(insn);
            break;
        case rv_opcode::store:
            execute_store(insn);
            break;
        case rv_opcode::amo:
            execute_amo(insn);
            break;
        case rv_opcode::op:
            execute_op(insn);
            break;
        case rv_opcode::lui:
            execute_lui(insn);
            break;
        case rv_opcode::branch:
            execute_branch(insn);
            break;
        case rv_opcode::jalr:
            execute_jalr(insn);
            break;
        case rv_opcode::jal:
            execute_jal(insn);
            break;
        case rv_opcode::system:
            execute_system(insn);
            break;
        default:
            raise_illegal_instruction();
            break;
        }
    }
    if (unlikely(exception_raised_)) {
        mcause_ = (uint32_t) exception_code_;
        switch (exception_code_) {
        case rv_exception::illegal_instruction:
            if (!memory_.read(pc_, mvtval_)) {
                mvtval_ = std::numeric_limits<decltype(mvtval_)>::max();
            }
            break;
        case rv_exception::store_access_fault:
        case rv_exception::load_access_fault:
        case rv_exception::instruction_access_fault:
            mvtval_ = memory_.faultAddress();
            break;
        default:
            mvtval_ = 0;
            break;
        }

        mepc_ = pc_ + 4;
        // save previous interrupt enable flag
        mstatus_ = (mstatus_ & ~RV_MSTATUS_MPIE) |
                   (((mstatus_ >> priv_) & 1) << RV_MSTATUS_MPIE_SHIFT);

        // save previous privilege level
        mstatus_ = (mstatus_ & ~RV_MSTATUS_MPP) |
                   (priv_ << RV_MSTATUS_MPP_SHIFT);

        // exception are always taken with interrupts disabled
        mstatus_ &= ~RV_MSTATUS_MIE;

        // switch privilege level to machine mode
        // we don't support delegated exceptions and/or interrupts
        // exceptions are always taken at Machine Level
        priv_ = RV_PRIV_M;

        // transfer execution to machine mode exception handler
        pc_ = mtvec_;
    }
    cycle_ += nCycles - c;
}

void rv_cpu::execute_lui(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    regs_[rd] = insn & 0xFFFFF000;
    next_insn();
}

void rv_cpu::execute_auipc(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    regs_[rd] = pc_ + (int32_t)(insn & 0xFFFFF000);
    next_insn();
}

void rv_cpu::execute_jal(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    rv_int imm = (bits(insn, 21, 30) << 1) |
                 (bit(insn, 20) << 11) |
                 (bits(insn, 12, 19) << 12) |
                 (bit(insn, 31) << 20);
    imm = (imm << 11) >> 11;
    if (rd != 0) {
        regs_[rd] = pc_ + 4;
    }
    jump_insn(pc_ + imm);
}

void rv_cpu::execute_jalr(uint32_t insn)
{
    const rv_int imm = (rv_int)insn >> 20;
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);

    if (rd != 0) {
        regs_[rd] = pc_ + 4;
    }
    jump_insn((regs_[rs1] + imm) & 0xFFFFFFFE);
}

void rv_cpu::execute_branch(uint32_t insn)
{
    const auto rs1 = decode_rs1(insn);
    const auto rs2 = decode_rs2(insn);
    const auto funct3 = decode_funct3(insn);

    const rv_uint val1 = regs_[rs1];
    const rv_uint val2 = regs_[rs2];
    int cond = 0;
    switch (funct3 >> 1) {
        case 0b00:  // beq | bne
            cond = val1 == val2;
            break;
        case 0b10:  // blt | bge
            cond = (rv_int)val1 < (rv_int)val2;
            break;
        case 0b11:  // bltu | bgeu
            cond = val1 < val2;
            break;
    }
    cond ^= (funct3 & 1);
    if (cond != 0) {
        rv_int imm = (bits(insn, 8, 11) << 1) |
                     (bits(insn, 25, 30) << 5) |
                     (bit(insn, 7) << 11) |
                     (bit(insn, 31) << 12);
        imm = (imm << 19) >> 19;
        jump_insn((pc_ + imm) & 0xFFFFFFFE);
    }
    else {
        next_insn();
    }
}

void rv_cpu::execute_load(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto funct3 = decode_funct3(insn);

    const rv_int imm = (rv_int)insn >> 20;
    const rv_uint addr = regs_[rs1] + imm;
    rv_uint val;
    switch (funct3) {
        case 0b000:  // lb
            int8_t i8;
            if (!memory_.read(addr, i8)) {
                raise_memory_exception();
                return;
            }
            val = i8;
            break;
        case 0b001:  // lh
            int16_t i16;
            if (!memory_.read(addr, i16)) {
                raise_memory_exception();
                return;
            }
            val = i16;
            break;
        case 0b010:  // lw
            int32_t i32;
            if (!memory_.read(addr, i32)) {
                raise_memory_exception();
                return;
            }
            val = i32;
            break;
        case 0b100:  // lbu
            uint8_t u8;
            if (!memory_.read(addr, u8)) {
                raise_memory_exception();
                return;
            }
            val = u8;
            break;
        case 0b101:  //lhu
            uint16_t u16;
            if (!memory_.read(addr, u16)) {
                raise_memory_exception();
                return;
            }
            val = u16;
            break;
        default:
            raise_illegal_instruction();
            return;
    }
    if (likely(rd != 0))
        regs_[rd] = val;
    next_insn();
}

void rv_cpu::execute_store(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto rs2 = decode_rs2(insn);
    const auto funct3 = decode_funct3(insn);

    const rv_int imm = (rv_int)((insn & 0xFE000000) | (rd << 20)) >> 20;
    const rv_uint addr = regs_[rs1] + imm;
    const rv_uint val = regs_[rs2];
    switch (funct3) {
        case 0b000:  // sb
            if (!memory_.write(addr, (uint8_t)(val & 0xFF))) {
                raise_memory_exception();
            }
            break;
        case 0b001:  // sh
            if (!memory_.write(addr, (uint16_t)(val & 0xFFFF))) {
                raise_memory_exception();
                return;
            }
            break;
        case 0b010:  // sw
            if (!memory_.write(addr, val)) {
                raise_memory_exception();
                return;
            }
            break;
        default:
            raise_illegal_instruction();
            return;
    }
    next_insn();
}

void rv_cpu::execute_imm(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto funct3 = decode_funct3(insn);

    const rv_int imm = (rv_int)insn >> 20;
    rv_uint res = 0;
    const rv_uint val = regs_[rs1];
    switch (funct3) {
        case 0b000:  // addi
            res = val + imm;
            break;
        case 0b001:  // slli
/*            if ((imm & 0x5F) != 0) {
                raise_illegal_instruction();
                return;
            }*/
            res  = val << (imm & 0x1F);
            break;
        case 0b010:  // slti
            res = (rv_int)val < imm ? 1 : 0;
            break;
        case 0b011:  // sltiu
            res = val < imm ? 1 : 0;
            break;
        case 0b100:
            res = val ^ imm;
            break;
        case 0b101:  // srai | srli
        {
            if ((imm & 0xFFFFFBE0) != 0) {
                raise_illegal_instruction();
                return;
            }
            if ((imm & 0x400) != 0)
                res = (rv_int)val >> (imm & 0x1F);
            else
                res = val >> (imm & 0x1F);
            break;
        }
        case 0b110:  // ori
            res = val | imm;
            break;
        case 0b111:  // andi
            res = val & imm;
            break;
    }
    if (likely(rd != 0))
        regs_[rd] = res;
    next_insn();
}

void rv_cpu::execute_op(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto rs2 = decode_rs2(insn);
    const auto funct3 = decode_funct3(insn);

    const rv_uint imm = insn >> 25;
    const rv_uint val1 = regs_[rs1];
    const rv_uint val2 = regs_[rs2];
    rv_uint res = 0;
    if (imm == 1) {
        const auto sval1 = (rv_int)val1;
        const auto sval2 = (rv_int)val2;
        switch (funct3) {
        case 0b000:  // mul
            res = (rv_uint)((rv_long)sval1 * (rv_long)sval2);
            break;
        case 0b001:  // mulh
            res = (rv_uint)(((rv_long)sval1 * (rv_long)sval2) >> 32);
            break;
        case 0b010:  // mulhsu
            res = (rv_uint)(((rv_long)sval1 * (rv_long)val2) >> 32);
            break;
        case 0b011:  // mulhu
            res = (rv_uint)(((rv_long)val1 * (rv_long)val2) >> 32);
            break;
        case 0b100:  // div
        {
            if (val2 == 0)
                res = (rv_uint)-1;
            else if (val1 == 0x80000000 && val2 == (rv_int)-1)
                res = val1;
            else
                res = sval1 / sval2;
        }
        break;

        case 0b101:  // divu
            if (val2 == 0)
                res = (rv_uint)-1;
            else
                res = val1 / val2;
            break;
        case 0b110:  // rem
        {
            if (val2 == 0)
                res = val1;
            else if (val1 == 0x80000000 && val2 == (rv_int)-1)
                res = 0;
            else
                res = (rv_uint)(sval1 % sval2);
        }
        break;
        case 0b111:  // remu
            if (val2 == 0)
                res = val1;
            else
                res = val1 % val2;
            break;
        }
    }
    else {
        switch (funct3) {
        case 0b000:  // add | sub
        {
            if (unlikely((imm & 0x5F) != 0)) {
                raise_illegal_instruction();
                return;
            }
            if ((imm & 0x20) != 0)  // sub
                res = val1 - val2;
            else  // add
                res = val1 + val2;
        }
        break;
        case 0b001:  // sll
            res = val1 << val2;
            break;
        case 0b010:  // slt
            res = (rv_int)val1 < (rv_int)val2 ? 1 : 0;
            break;
        case 0b011:  // sltu
            res = val1 < val2 ? 1 : 0;
            break;
        case 0b100:  // xor
            res = val1 ^ val2;
            break;
        case 0b101:  // srl | sra
        {
            if ((imm & 0x5F) != 0) {
                raise_illegal_instruction();
                return;
            }
            if ((imm & 0x20) != 0)  // sra
                res = (rv_uint)((rv_int)val1 >> val2);
            else  // srl
                res = val1 >> val2;
        }
        break;
        case 0b110:  // or
            res = val1 | val2;
            break;
        case 0b111:  // and
            res = val1 & val2;
            break;
        }
    }
    if (likely(rd != 0)) {
        regs_[rd] = res;
    }
    next_insn();
}

void rv_cpu::execute_amo(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto rs2 = decode_rs2(insn);
    const auto funct5 = insn >> 27;

    const auto addr = regs_[rs1];
    rv_uint val;
    switch (funct5) {
    case 0b00010:  // lr.w
        if (rs2 != 0) {
            raise_illegal_instruction();
            return;
        }
        int32_t i32;
        if (!memory_.read(addr, i32)) {
            raise_memory_exception();
            return;
        }
        val = i32;
        amo_res_ = addr;
        break;

    case 0b00011:  // sc.w
        if (amo_res_ == addr) {
            if (!memory_.write(addr, regs_[rs1])) {
                raise_memory_exception();
                return;
            }
            val = 0;
        }
        else {
            val = 1;
        }
        break;
    case 0b00001:  // amoswap.w
    case 0b00000:  // amoadd.w
    case 0b00100:  // amoxor.w
    case 0b01100:  // amoand.w
    case 0b01000:  // amoor.w
    case 0b10000:  // amomin.w
    case 0b10100:  // amomax.w
    case 0b11000:  // amominu.w
    case 0b11100:  // amomaxu.w
    {
        if (memory_.read(addr, (int32_t&)val)) {
            raise_memory_exception();
            return;
        }
        auto val2 = regs_[rs2];
        switch (funct5) {
        case 0b00001:  // amoswap.w
            break;
        case 0b00000:  // amoadd.w
            val2 = (int32_t)(val + val2);
            break;
        case 0b00100:  // amoxor.w
            val2 = (int32_t)(val ^ val2);
            break;
        case 0b01100:  // amoand.w
            val2 = (int32_t)(val & val2);
            break;
        case 0b01000:  // amoor.w
            val2 = (int32_t)(val | val2);
            break;
        case 0b10000:  // amomin.w
            if ((int32_t)val < (int32_t)val2)
                val2 = (int32_t)val;
            break;
        case 0b10100:  // amomax.w
            if ((int32_t)val > (int32_t)val2)
                val2 = (int32_t)val;
            break;
        case 0b11000:  // amominu.w
            if ((uint32_t)val < (uint32_t)val2)
                val2 = (int32_t)val;
            break;
        case 0b11100:  // amomaxu.w
            if ((uint32_t)val > (uint32_t)val2)
                val2 = (int32_t)val;
            break;

        }
        if (!memory_.write(addr, val2)) {
            raise_memory_exception();
            return;
        }
        if (rd != 0) {
            regs_[rd] = val;
        }
    }
    break;
    default:
        raise_illegal_instruction();
        return;
    }
    next_insn();
}
void rv_cpu::execute_misc_mem(uint32_t insn)
{
    // fence and fence.i are nops
    next_insn();
}

void rv_cpu::execute_system(uint32_t insn)
{
    const auto rd = decode_rd(insn);
    const auto rs1 = decode_rs1(insn);
    const auto funct3 = decode_funct3(insn);
    const uint32_t imm = insn >> 20;

    switch (funct3) {
    case 0:  // ecall | ebreak | mret
        switch (imm) {
        case 0:  // ecall
            if ((insn & 0x000FFF80) != 0)
                raise_illegal_instruction();
            raise_exception(static_cast<rv_exception>(
                                static_cast<uint32_t>(rv_exception::ecall_from_umode) +
                                static_cast<uint32_t>(priv_))
            );
            return;
        case 1:  // ebreak
            if ((insn & 0x000FFF80) != 0)
                raise_illegal_instruction();
            raise_exception(rv_exception::breakpoint);
            return;
        case 0x302:  // mret
            if ((insn & 0x000FFF80) != 0 || priv_ < RV_PRIV_M)
                raise_illegal_instruction();
            execute_mret();
            return;
        default:
            raise_illegal_instruction();
            return;
        }
        break;

    case 1:  // csrrw
    case 2:  // csrrs
    case 3:  // csrrc
        if (!csr_rw(imm, rd, regs_[rs1], funct3))
            return;
        break;
    case 5:  // csrrwi
    case 6:  // csrrsi
    case 7:  // csrrci
        if (!csr_rw(imm, rd, (rv_uint)(rs1 & 0b11111), funct3 - 4))
            return;
        break;
    default:
        raise_illegal_instruction();
        return;
    }

    next_insn();
}

bool rv_cpu::csr_read(uint32_t csr, rv_uint &csr_value, bool write_back)
{
    // these are read-only CSRs
    if ((csr & 0xC00) == 0xC00 && write_back) {
        raise_illegal_instruction();
        return false;
    }

    // check privilege level
    if (priv_ < ((csr >> 8) & 3)) {
        raise_illegal_instruction();
        return false;
    }

    switch ((rv_csr)csr) {
    case rv_csr::cycle:
    case rv_csr::instret:
        if (priv_ < RV_PRIV_M) {
            if ((mcounteren_ & RV_MCOUNTEREN_CY) == 0) {
                raise_illegal_instruction();
                return false;
            }
        }
        csr_value = (rv_uint)(cycle_ & 0xFFFFFFFF);
        break;
    case rv_csr::cycleh:
    case rv_csr::instreth:
        if (priv_ < RV_PRIV_M) {
            if ((mcounteren_ & RV_MCOUNTEREN_IR) == 0) {
                raise_illegal_instruction();
                return false;
            }
        }
        csr_value = (rv_uint)(cycle_ >> 32);
        break;
    case rv_csr::mstatus:
        csr_value = mstatus_;
        break;
    case rv_csr::misa:
        csr_value = misa_;
        break;
    case rv_csr::mie:
        csr_value = mie_;
        break;
    case rv_csr::mip:
        csr_value = mip_;
        break;
    case rv_csr::mtvec:
        csr_value = mtvec_;
        break;
    case rv_csr::mcounteren:
        csr_value = mcounteren_ & (RV_MCOUNTEREN_CY | RV_MCOUNTEREN_TM | RV_MCOUNTEREN_IR);
        break;
    case rv_csr::mscratch:
        csr_value = mscratch_;
        break;
    case rv_csr::mepc:
        csr_value = mepc_;
        break;
    case rv_csr::mcause:
        csr_value = mcause_;
        break;
    case rv_csr::mtval:
        csr_value = mvtval_;
        break;
    case rv_csr::mvendorid:
    case rv_csr::mimpid:
    case rv_csr::mhartid:
        csr_value = 0;
        break;
    default:
        raise_illegal_instruction();
        return false;
    }
    return true;
}

bool rv_cpu::csr_write(uint32_t csr, rv_uint csr_value)
{
    uint32_t mask;
    switch ((rv_csr)csr) {
    case rv_csr::mstatus:
        // no support for TLB flush
        // no support for SXL/UXL (RV32 mode only)
        // no support for floating point unit
        mstatus_ = csr_value;
        break;
    case rv_csr::misa:
        return false;
    case rv_csr::mie:
        // we can only set Machine Mode interrupts
        mask = RV_MIE_MSIE | RV_MIE_MTIE | RV_MIE_MEIE;
        mie_ = (mie_ & ~mask) | (csr_value & mask);
        break;
    case rv_csr::mip:
        // we don't support supervisor mode or user mode interrupts
        // so do nothing here
        break;
    case rv_csr::mtvec:
        mtvec_ = csr_value;
        break;
    case rv_csr::mcounteren:
        mask = RV_MCOUNTEREN_CY | RV_MCOUNTEREN_TM | RV_MCOUNTEREN_IR;
        mcounteren_ = (mcounteren_ & ~mask) | (csr_value & mask);
        break;
    case rv_csr::mscratch:
        mscratch_ = csr_value;
        break;
    case rv_csr::mepc:
        mepc_ = csr_value & (~1);
        break;
    case rv_csr::mcause:
        mcause_ = csr_value;
        break;
    case rv_csr::mtval:
        mvtval_ = csr_value;
        break;
    default:
        raise_illegal_instruction();
        return false;
    }
    return true;
}

bool rv_cpu::csr_rw(uint32_t csr, uint32_t rd, rv_uint new_value, uint32_t csrop)
{
    rv_uint csrvalue = 0;
    switch (csrop) {
    case 1:
        if (!csr_read(csr, csrvalue, true)) {
            return false;
        }
        if(!csr_write(csr, new_value)) {
            return false;
        }
        break;
    case 2:
        if (!csr_read(csr, csrvalue, new_value != 0)) {
            return false;
        }
        if (new_value != 0) {
            if(!csr_write(csr, csrvalue & ~new_value)) {
                return false;
            }
        }
        break;
    case 3:
        if (!csr_read(csr, csrvalue, new_value != 0)) {
            return false;
        }
        if (new_value != 0) {
            if (!csr_write(csr, csrvalue | new_value)) {
                return false;
            }
        }
        break;
    default:
        return false;
    }
    if (rd != 0) {
        regs_[rd] = csrvalue;
    }
    return true;
}

void rv_cpu::execute_mret()
{
    uint32_t mpp = (mstatus_ >> RV_MSTATUS_MPP_SHIFT) & 3;
    uint32_t mpie = (mstatus_ >> RV_MSTATUS_MPIE_SHIFT) & 1;
    mstatus_ = (mstatus_ & ~(1 << mpp)) |
        (mpie << mpp);

    mstatus_ |= RV_MSTATUS_MPIE;
    mstatus_ &= ~RV_MSTATUS_MPP;
    priv_ = mpp;
    pc_ = mepc_;
}

void rv_cpu::raise_exception(rv_exception code)
{
    exception_code_ = code;
    exception_raised_ = true;
}

void rv_cpu::raise_interrupt()
{
    // raise interrupt if interrupts are enabled globally (mstatus.MIE)
    // and according to interrupt pending/enable mask
    const uint32_t mask = mie_ & mip_;
    if (mask == 0 || (mstatus_ & RV_MSTATUS_MIE) == 0)
        return;

    raise_exception(static_cast<rv_exception>((1U << 31) | __builtin_ctz(mask)));
}

void rv_cpu::update_mip(uint32_t irq_num, bool state)
{
    if (state)
        mip_ |= (1U << irq_num);
    else
        mip_ &= ~(1U << irq_num);
}