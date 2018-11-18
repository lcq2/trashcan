#pragma once
#include <array>
#include <rv_device.hpp>

class rv_plic : public rv_device
{
public:
    rv_plic() = delete;

    rv_plic(std::string _device_name, rv_uint _base_address, rv_uint _top_address)
        : rv_device(std::move(_device_name), _base_address, _top_address)
    {
        sources_.fill(nullptr);
    }

    void reset() override {}
    void attach(rv_device *dev, uint32_t int_number);
/*    void detach(uint32_t int_number);
*/
    bool read_u32(uint32_t regNo, uint32_t& value) override;
    bool write_u32(uint32_t regNo, uint32_t value) override;

private:
    void signal_plic(uint32_t int_number, bool state);
    void check_interrupt();

private:
    std::array<rv_device*, 32> sources_;
    uint32_t pending_;
    uint32_t served_;

    // no priority supported for now
    // interrupts are either enabled or disabled
    uint32_t enabled_;
};