#include "rv_device.hpp"

void rv_device::connect_irq(std::function<void(uint32_t,bool)> irq, uint32_t irq_number)
{
    irq_ = std::move(irq);
    irq_number_ = irq_number;
}

void rv_device::disconnect_irq()
{
    irq_ = nullptr;
}

void rv_device::set_irq(bool l)
{
    if (irq_)
        irq_(irq_number_, l);
}