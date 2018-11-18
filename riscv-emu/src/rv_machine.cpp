#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include "rv_machine.hpp"
#include <chrono>
#include <iostream>
#include <ratio>
#include <limits>
#include <thread>
#include <fcntl.h>

rv_machine::rv_machine()
    : memory_{16_MiB}, cpu_{memory_}
{
    memory_.attach(&uart0_);
    plic_.attach(&uart0_, 1);

    plic_.connect_irq(std::bind(&rv_cpu::update_mip, &cpu_, std::placeholders::_1, std::placeholders::_2), 11);
    cpu_.reset();
}

void rv_machine::loadBinary(const std::string& filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) < 0)
        throw std::runtime_error("stat failed");

    FILE *f = fopen(filename.c_str(), "r");
    if (f == nullptr)
        throw std::runtime_error("file not found");
    
    std::vector<uint8_t> buf(st.st_size);
    fread(buf.data(), sizeof(uint8_t), st.st_size, f);
    fclose(f);

    memory_.load(0x1000, buf.data(), buf.size());
}

void rv_machine::process_devices()
{
    fd_set rfds;
    struct timeval tv;
    std::array<uint8_t, 32> buf;

    // update uart0, used as "terminal" here
    if (uart0_.can_write()) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        int res = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
        if (res > 0 && FD_ISSET(STDIN_FILENO, &rfds)) {
            size_t l = std::min(uart0_.write_len(), buf.size());
            ssize_t bytes_read = read(STDIN_FILENO, buf.data(), l);
            if (bytes_read > 0) {
                uart0_.write_data(buf.data(), bytes_read);
            }
        }
    }
    if (uart0_.can_read()) {
        size_t l = uart0_.read_len();
        if (l > 0) {
            l = std::min(l, buf.size());
            uart0_.read_data(buf.data(), l);
            fwrite(buf.data(), sizeof(uint8_t), l, stdout);
            fflush(stdout);
        }
    }
}

#define TIMEIT
void rv_machine::run()
{
    using namespace std::chrono_literals;

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

#ifdef TIMEIT
    std::thread([this](){
        uint64_t prev_cycle = cpu_.cycle_count();

        for (;;) {
            std::this_thread::sleep_for(1s);
            uint64_t cur_cycle = cpu_.cycle_count();
            uint64_t delta = cur_cycle - prev_cycle;
            prev_cycle = cur_cycle;
            fprintf(stderr, "IPS: %lld\n", delta/1000);
        }
    }).detach();

    for (;;) {
        process_devices();
        cpu_.run(5000);
    }
#elif defined(PROFILE)
    for (int i = 0; i < 2000; ++i) {
        cpu_.run(500000);
    }
#else

    for (int i = 0; i < 200; ++i) {
        cpu_.run(10);
    }
#endif
}