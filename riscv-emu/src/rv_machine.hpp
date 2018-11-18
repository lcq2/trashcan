#pragma once
#include <string>
#include "rv_memory.hpp"
#include "rv_cpu.hpp"
#include "devices/rv_plic.hpp"
#include "devices/rv_uart.hpp"

class rv_machine
{
public:
    rv_machine();

    void loadBinary(const std::string& filename);
    void run();

    rv_memory& memory() { return memory_; }

private:
    void process_devices();

private:
    rv_memory memory_;
    rv_cpu cpu_;
    /* rv_clint clint_ */

    // I believe the PLIC address space needs some serious tuning....
    rv_plic plic_{"plic", 0xC1000000, 0xC1200000};

    rv_uart uart0_{"uart0", 0xC2000000, 0xC2000FFF};
/*    rv_uart uart1_{"uart1", 0x83000000, 0x83000FFF};*/
};