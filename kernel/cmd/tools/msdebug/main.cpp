#include "msdebug.hpp"

extern "C" void msdebug_main()
{
    msdebug::init();
    msdebug::shell();
}
