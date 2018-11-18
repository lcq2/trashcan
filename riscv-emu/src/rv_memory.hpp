#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <type_traits>
#include "rv_global.hpp"
#include "rv_exceptions.hpp"
#include "rv_device.hpp"

constexpr rv_uint RV_MEMORY_RAM_BEGIN = 0x00000000;
constexpr rv_uint RV_MEMORY_RAM_END = 0xC0000000;

class rv_memory
{
public:
    rv_memory() = delete;
    rv_memory(rv_uint ram_size);
    ~rv_memory();

    void load(rv_uint address, const uint8_t *data, size_t len);
    void dump(rv_uint address, uint8_t *outm, size_t len) const;

    bool attach(rv_device *device);

    // why would you do that!!!??? :D
//    void detach(const std::string& deviceName);
//    void detach(rv_uint address);

    rv_uint faultAddress() const { return m_faultAddress; }
    rv_exception lastException() const { return m_lastException; }

    bool prefetch_code(rv_uint address, uint32_t *insns, size_t count)
    {
        if (likely(address <= (m_ramEnd - sizeof(uint32_t)*count) && (rv_int)address > 0)) {
            memcpy(insns, m_ram + address, sizeof(uint32_t)*count);
            return true;
        }
        m_faultAddress = address;
        m_lastException = rv_exception::instruction_access_fault;
        return false;
    }

    template<typename T> bool read(rv_uint address, T& value) const
    {
        if (address <= (m_ramEnd - sizeof(T))) {
            value = *(T *)(m_ram + address);
            return true;
        }
        else if (address >= RV_MEMORY_RAM_END) {
            auto device = m_devices[getDeviceId(address)];
            if (device != nullptr) {
                if constexpr(sizeof(T) == 1) {
                    return device->read_u8(address & 0xFFFFFF, (uint8_t&)value);
                }
                if constexpr(sizeof(T) == 2) {
                    return device->read_u16(address & 0xFFFFFF, (uint16_t&)value);
                }
                if constexpr(sizeof(T) == 4) {
                    return device->read_u32(address & 0xFFFFFF, (uint32_t&)value);
                }
            }
        }
        m_faultAddress = address;
        m_lastException = rv_exception::load_access_fault;
        return false;
    }

    template<typename T> bool write(rv_uint address, T value)
    {
        if (address <= (m_ramEnd - sizeof(T))) {
            *(T *)(m_ram + address) = value;
            return true;
        }
        else if (address >= RV_MEMORY_RAM_END) {
            auto device = m_devices[getDeviceId(address)];
            if (device != nullptr) {
                if constexpr(sizeof(T) == 1) {
                    return device->write_u8(address & 0xFFFFFF, value);
                }
                if constexpr(sizeof(T) == 2) {
                    return device->write_u16(address & 0xFFFFFF, value);
                }
                if constexpr(sizeof(T) == 4) {
                    return device->write_u32(address & 0xFFFFFF, value);
                }
            }
        }
        m_faultAddress = address;
        m_lastException = rv_exception::store_access_fault;
        return false;
    }

private:
    size_t getDeviceId(rv_uint address) const { return (address >> 24) & 0xF; }

private:
    uint8_t *m_ram;
    rv_uint m_ramBegin;
    rv_uint m_ramEnd;

    // up to 16 devices supported for now
    std::array<rv_device*, 16> m_devices;

    mutable rv_uint m_faultAddress;
    mutable rv_exception m_lastException;
};