#include "kernel/io_uring.hpp"

void example_io_uring()
{
    Blockos::io_uring ring(8);
    ring.start();

    if (ring.submit(/*opcode*/ 0, /*fd*/ -1, /*addr*/ 0, /*len*/ 0, /*user_data*/ 1)) {
        ring.poll();

        uint64_t user_data = 0;
        int32_t result = 0;
        (void)ring.get_completion(user_data, result);
    }

    ring.stop();
}
