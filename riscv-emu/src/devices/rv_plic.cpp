#include <cassert>
#include <functional>
#include "rv_plic.hpp"

void rv_plic::attach(rv_device *dev, uint32_t int_number)
{
    if (int_number <= 32) {
        sources_[int_number] = dev;
        dev->connect_irq(
            std::bind(&rv_plic::signal_plic, this, std::placeholders::_1, std::placeholders::_2),
            int_number
            );
    }
}

void rv_plic::signal_plic(uint32_t int_number, bool state)
{
    uint32_t mask = 1U << (int_number - 1);
    if (state)
        pending_ |= mask;
    else
        pending_ &= ~mask;

    check_interrupt();
}

void rv_plic::check_interrupt()
{
    set_irq((pending_ & served_) != 0);
}

bool rv_plic::read_u32(uint32_t regNo, uint32_t& value)
{
    switch (regNo) {
    case 0:
        value = 0;
        break;
    case 4: {
        uint32_t mask = pending_ & ~served_;
        if (mask != 0) {
            uint32_t i = __builtin_ctz(mask);
            served_ |= (1U << i);
            check_interrupt();
            value = i + 1;
        }
        else {
            value = 0;
        }
    }
    break;

    default:
        value = 0;
        break;
    }

    return true;
}

bool rv_plic::write_u32(uint32_t regNo, uint32_t value)
{
    switch (regNo) {
    case 4:
        value--;
        if ( value < 32) {
            served_ &= ~(1U << value);
            check_interrupt();
        }
        break;
    default:
        break;
    }

    return true;
}