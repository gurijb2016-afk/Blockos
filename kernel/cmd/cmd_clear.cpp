#include "command.hpp"

namespace blockos::cmd
{

extern "C" int clear_main(const Args& args, Console& out)
{
    (void)args;

    out.clear();

    return 0;
}

} // namespace blockos::cmd
