#include "server.hpp"

// Userspace entry point placeholder for the eventual BlockOS process wrapper.
// The process/IPC runtime will call Server::process_one() once BlockOS has
// a real userspace socket/event loop. Keeping this binary stub separate from
// the freestanding kernel prevents accidental hosted-library dependencies.
int main() { return 0; }
