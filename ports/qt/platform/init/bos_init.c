/* BlockOS startup: run the .init_array constructors (static ctors, Q_IMPORT_PLUGIN registrations), then call main. */
#include <stddef.h>
typedef void (*bos_initfn)(int, char **, char **);
extern bos_initfn __init_array_start[] __attribute__((weak, visibility("hidden")));
extern bos_initfn __init_array_end[] __attribute__((weak, visibility("hidden")));
extern int main(int, char **, char **);
int bos_start_main(int argc, char **argv, char **envp) {
  size_t n = (size_t)(__init_array_end - __init_array_start);
  for (size_t i = 0; i < n; i++) __init_array_start[i](argc, argv, envp);
  return main(argc, argv, envp);
}
