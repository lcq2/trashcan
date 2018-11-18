#include <cstring>
#include <cassert>
#include "rv_exceptions.hpp"
#include "rv_memory.hpp"

rv_memory::rv_memory(rv_uint ram_size)
{
    m_ram = new uint8_t[ram_size];
    m_ramBegin = RV_MEMORY_RAM_BEGIN;
    m_ramEnd = ram_size;
    m_devices.fill(nullptr);
}

rv_memory::~rv_memory()
{
    if (m_ram != nullptr) {
        delete [] m_ram;
    }
}

void rv_memory::load(rv_uint address, const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0)
        return;

    memcpy(m_ram + address, data, len);
}

void rv_memory::dump(rv_uint address, uint8_t *outm, size_t len) const
{
    if (outm == nullptr || len == 0)
        return;

    memcpy(outm, m_ram, len);
}

bool rv_memory::attach(rv_device *device)
{
    if (device->base_address() < RV_MEMORY_RAM_END)
        return false;

    auto dev_id = (device->base_address() >> 24) & 0x0F;
    m_devices[dev_id] = device;
    return true;
}
/*
void rv_memory::detach(const std::string &deviceName)
{
    // not implemented
}*/