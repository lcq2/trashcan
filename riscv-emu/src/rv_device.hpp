#pragma once

#include <string>
#include <functional>
#include "rv_global.hpp"

class rv_device
{
protected:
    rv_device(std::string _device_name, rv_uint _base_address, rv_uint _top_address, uint32_t _int_number = 0)
        : device_name_{std::move(_device_name)},
        base_address_{_base_address},
        top_address_{_top_address},
        irq_number_{0}
    {

    }

public:
    rv_device() = delete;

    rv_device(const rv_device&) = delete;
    rv_device& operator=(const rv_device&) = delete;

    virtual ~rv_device() = default;

    virtual void reset() = 0;

    // memory-mapped io functions (always unsigned)
    // returns false by default, triggering a memory exception
    virtual bool read_u8(uint32_t regNo, uint8_t& value) { (void)value; return false; }
    virtual bool read_u16(uint32_t regNo, uint16_t& value) { (void)value; return false; }
    virtual bool read_u32(uint32_t regNo, uint32_t& value) { (void)value; return false; }

    virtual bool write_u8(uint32_t regNo, uint8_t value) { (void)value; return false; }
    virtual bool write_u16(uint32_t regNo, uint16_t value) { (void)value; return false; }
    virtual bool write_u32(uint32_t regNo, uint32_t value) { (void)value; return false; }

    void connect_irq(std::function<void(uint32_t,bool)> irq, uint32_t irq_number);
    void disconnect_irq();

    void set_irq(bool l);

    const std::string& device_name() const { return device_name_; }
    rv_uint base_address() const { return base_address_; }
    rv_uint top_address() const { return top_address_; }
    uint32_t irq_number() const { return irq_number_; }

private:
    std::string device_name_;
    rv_uint base_address_;
    rv_uint top_address_;
    std::function<void(uint32_t,bool)> irq_;
    uint32_t irq_number_;
};