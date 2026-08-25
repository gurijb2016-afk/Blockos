#include "command.hpp"

#include "proc.hpp"

namespace blockos::cmd
{

extern "C" int help_main(const Args& args, Console& out)
{
    if (args.count > 1)
    {
        const CommandEntry* entry = find_command(args.at(1));

        if (!entry)
        {
            out.print("no help for: ");
            out.print(args.at(1));
            out.newline();

            return 1;
        }

        out.print(entry->name);
        out.print(" - ");
        out.print(entry->help);
        out.newline();

        return 0;
    }

    for (size_t i = 0; i < command_count(); ++i)
    {
        const CommandEntry* entry = command_at(i);

        out.print(entry->name);
        out.print(" - ");
        out.print(entry->help);
        out.newline();
    }


    for (size_t i = 0; i < blockos::proc::count(); ++i)
    {
        out.print(blockos::proc::name_at(i));
        out.newline();
    }
    return 0;
}

} // namespace blockos::cmd
