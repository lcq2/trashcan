#include "rv_machine.hpp"

int main()
{
    rv_machine m;
    m.loadBinary("test.bin");
    m.memory().write(0x2000, (int32_t)-2);
    m.memory().write(0x2004, (int32_t)-3);

    m.run();

    rv_uint addr = 0x2008;
    uint32_t res;
    m.memory().read(0x2008, res);
    printf("%d\n", res);

    return 0;
}