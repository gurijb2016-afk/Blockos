#include "sysmem.hpp"

namespace sysmem
{
static SystemMemoryRecord record{};

const SystemMemoryRecord& get_record()
{
    return record;
}

void set_record(const SystemMemoryRecord& in)
{
    record = in;
}

} // namespace sysmem
