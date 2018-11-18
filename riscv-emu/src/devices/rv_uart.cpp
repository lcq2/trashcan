#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "rv_uart.hpp"

void rv_uart::reset()
{
    txctrl_ = 0;
    rxctrl_ = 0;
    ie_ = 0;
    ip_ = 0;
}

bool rv_uart::read_u32(uint32_t regNo, uint32_t& value)
{
    auto rxwm = (rxctrl_ & RV_UART_RXCTRL_RXCNT) >> RV_UART_RXCTRL_RXCNT;

    switch((uart_reg)regNo) {
    case uart_reg::txdata:
        value = 0;
        if (txfifo_.full())
            value |= 0x80000000;
        break;
    case uart_reg::rxdata:
        if (rxfifo_.empty())
            value = 0x80000000;
        else
            value = (uint32_t)rxfifo_.dequeue();
        if (rxfifo_.count() <= rxwm) {
            ip_ &= ~RV_UART_IP_RXWM;
//            set_irq((ie_ & ip_) != 0);
        }
        break;
    case uart_reg::rxctrl:
        value = rxctrl_;
        break;
    case uart_reg::txctrl:
        value = txctrl_;
        break;
    case uart_reg::div:
        break;
    case uart_reg::ip:
        value = ip_;
        break;
    case uart_reg::ie:
        value = ie_;
        break;
    default:
        return false;
    }
    return true;
}

bool rv_uart::write_u32(uint32_t regNo, uint32_t value)
{
    switch((uart_reg)regNo) {
    case uart_reg::txdata:
        if (!txfifo_.full())
            txfifo_.enqueue((uint8_t)(value & 0xFF));
        break;
    case uart_reg::rxdata:
        break;
    case uart_reg::txctrl:
        txctrl_ = value;
        break;
    case uart_reg::rxctrl:
        rxctrl_ = value;
        break;
    case uart_reg::div:
        break;
    case uart_reg::ie:
        ie_ = (ie_ & ~3) | (value & 3);
        break;
    case uart_reg::ip:
        break;
    default:
        return false;
    }
    return true;
}

void rv_uart::write_data(const uint8_t *data, size_t len)
{
    rxfifo_.put(data, len);
    auto rxwm = (rxctrl_ & RV_UART_RXCTRL_RXCNT) >> RV_UART_RXCTRL_RXCNT;

    if (rxfifo_.count() > rxwm) {
        ip_ |= RV_UART_IP_RXWM;
        set_irq((ie_ & ip_) != 0);
    }
}
/*
void rv_uart::update()
{
    // check if we can transmit
    if (txfifo_.count() > 0 && (txctrl_ & RV_UART_TXCTRL_TXEN) != 0) {

        // empty fifo
        while (txfifo_.count() > 0) {
            if (can_write_ && can_write_() && write_byte_)
                write_byte_(txfifo_.dequeue());
        }
        const auto txwm = (txctrl_ & RV_UART_TXCTRL_TXCNT) >> RV_UART_TXCTRL_TXCNT;
        if (txfifo_.count() < txwm)
            ip_ |= RV_UART_IP_TXWM;
        else
            ip_ &= ~RV_UART_IP_TXWM;
    }

    // check if we can receive
    if ((rxctrl_ & RV_UART_RXCTRL_RXEN) != 0 && read_byte_) {
        // receive 1 byte and eventually raise/clear interrupt
        if (can_read_ && can_read_()) {
            rxfifo_.enqueue(read_byte_());
            const auto rxwm = (rxctrl_ & RV_UART_RXCTRL_RXCNT) >> RV_UART_RXCTRL_RXCNT;
            if (rxfifo_.count() > rxwm)
                ip_ |= RV_UART_IP_RXWM;
            else
                ip_ &= RV_UART_IP_RXWM;
        }
    }

    // update irq state if necessary
    set_irq((ip_ & ie_) != 0);
}*/