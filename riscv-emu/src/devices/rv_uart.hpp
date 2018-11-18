#pragma once
#include <array>
#include <functional>
#include <string>
#include <cassert>
#include <rv_bits.hpp>
#include <rv_device.hpp>

enum class uart_reg
{
    txdata = 0x000,
    rxdata = 0x004,
    txctrl = 0x008,
    rxctrl = 0x00C,
    ie = 0x010,
    ip = 0x014,
    div = 0x018
};


constexpr auto RV_UART_TXCTRL_TXEN = rv_bitfield<1,0>{};
constexpr auto RV_UART_TXCTRL_NSTOP = rv_bitfield<1,1>{};
constexpr auto RV_UART_TXCTRL_TXCNT = rv_bitfield<3,16>{};
constexpr auto RV_UART_RXCTRL_RXEN = rv_bitfield<1,0>{};
constexpr auto RV_UART_RXCTRL_RXCNT = rv_bitfield<3,16>{};


constexpr auto RV_UART_IE_TXWM = rv_bitfield<1,0>{};
constexpr auto RV_UART_IE_RXWM = rv_bitfield<1,1>{};

constexpr auto RV_UART_IP_TXWM = rv_bitfield<1,0>{};
constexpr auto RV_UART_IP_RXWM = rv_bitfield<1,1>{};

template<std::size_t l>
class uart_fifo
{
public:
    uart_fifo() = default;

    bool full() const { return count_ == l; }
    bool empty() const { return count_ == 0; }

    void put(const uint8_t *data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
            enqueue(*data++);
    }

    void get(uint8_t *data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
            data[i] = dequeue();
    }

    void enqueue(uint8_t b)
    {
        buf_[tail_] = b;
        tail_ = (tail_ + 1) % l;
        ++count_;
    }
    uint8_t dequeue()
    {
        const auto val = buf_[head_];
        head_ = (head_+1) % l;
        --count_;
        return val;
    }

    size_t count() const { return count_; }
    size_t free_space() const { return l - count_; }
private:
    std::array<uint8_t, l> buf_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

class rv_uart : public rv_device
{
public:
    rv_uart() = delete;
    rv_uart(std::string _device_name, rv_uint _base_address, rv_uint _top_address)
        : rv_device(std::move(_device_name), _base_address, _top_address)
    {
        reset();
    }

    void reset() override;

    void write_data(const uint8_t *data, size_t len);
    bool can_write() const { return !rxfifo_.full() && (rxctrl_ & RV_UART_RXCTRL_RXEN) != 0; }
    size_t write_len() const { return rxfifo_.free_space(); }

    void read_data(uint8_t *data, size_t len) { txfifo_.get(data, len); }
    bool can_read() const { return !txfifo_.empty() && (txctrl_ & RV_UART_TXCTRL_TXEN) != 0; }
    size_t read_len() const { return txfifo_.count(); }

    // read/write from the cpu (only 32bit mmio is supported for uart)
    bool read_u32(uint32_t regNo, uint32_t& value) override;
    bool write_u32(uint32_t regNo, uint32_t value) override;

private:
    uart_fifo<8> rxfifo_;
    uart_fifo<8> txfifo_;
    uint32_t txctrl_ = 0;
    uint32_t rxctrl_ = 0;
    uint32_t ip_ = 0;
    uint32_t ie_ = 0;
};