#include "command.hpp"
#include <array>
#include <iostream>
#include <string>

#define DECLARE_CMD(name) extern "C" int name##_main(int argc, char** argv);

namespace blockos::cmd {

DECLARE_CMD(install) DECLARE_CMD(reboot) DECLARE_CMD(shutdown) DECLARE_CMD(mount)
DECLARE_CMD(umount) DECLARE_CMD(ls) DECLARE_CMD(cd) DECLARE_CMD(pwd)
DECLARE_CMD(cat) DECLARE_CMD(echo) DECLARE_CMD(touch) DECLARE_CMD(mkdir)
DECLARE_CMD(rmdir) DECLARE_CMD(cp) DECLARE_CMD(mv) DECLARE_CMD(rm)
DECLARE_CMD(chmod) DECLARE_CMD(chown) DECLARE_CMD(whoami) DECLARE_CMD(id)
DECLARE_CMD(clear) DECLARE_CMD(help) DECLARE_CMD(time) DECLARE_CMD(date)
DECLARE_CMD(top) DECLARE_CMD(free) DECLARE_CMD(df) DECLARE_CMD(du)
DECLARE_CMD(ip) DECLARE_CMD(ping) DECLARE_CMD(netstat) DECLARE_CMD(route)
DECLARE_CMD(ifconfig) DECLARE_CMD(login) DECLARE_CMD(logout) DECLARE_CMD(useradd)
DECLARE_CMD(userdel) DECLARE_CMD(groupadd) DECLARE_CMD(groupdel) DECLARE_CMD(su)
DECLARE_CMD(sudo) DECLARE_CMD(service) DECLARE_CMD(start) DECLARE_CMD(stop)
DECLARE_CMD(restart) DECLARE_CMD(status) DECLARE_CMD(log) DECLARE_CMD(journal)
DECLARE_CMD(sysinfo) DECLARE_CMD(procinfo) DECLARE_CMD(meminfo) DECLARE_CMD(blkinfo)
DECLARE_CMD(devinfo) DECLARE_CMD(fdinfo) DECLARE_CMD(env) DECLARE_CMD(export)
DECLARE_CMD(unset) DECLARE_CMD(set) DECLARE_CMD(get) DECLARE_CMD(alias)
DECLARE_CMD(unalias) DECLARE_CMD(history) DECLARE_CMD(sleep) DECLARE_CMD(watch)
DECLARE_CMD(find) DECLARE_CMD(grep) DECLARE_CMD(sort) DECLARE_CMD(head)
DECLARE_CMD(tail) DECLARE_CMD(diff) DECLARE_CMD(patch) DECLARE_CMD(test)
DECLARE_CMD(benchmark) DECLARE_CMD(version) DECLARE_CMD(pkg) DECLARE_CMD(update)

static const std::array<CommandEntry, 76> kCommands = {{
    {"install", install_main, "Install BlockOS image to a target disk or directory."},
    {"reboot", reboot_main, "Reboot the machine."},
    {"shutdown", shutdown_main, "Power off the machine."},
    {"mount", mount_main, "Mount a filesystem."},
    {"umount", umount_main, "Unmount a filesystem."},
    {"ls", ls_main, "List directory entries."},
    {"cd", cd_main, "Change current directory."},
    {"pwd", pwd_main, "Print current directory."},
    {"cat", cat_main, "Print file contents."},
    {"echo", echo_main, "Print arguments to stdout."},
    {"touch", touch_main, "Create or update a file."},
    {"mkdir", mkdir_main, "Create directories."},
    {"rmdir", rmdir_main, "Remove empty directories."},
    {"cp", cp_main, "Copy files."},
    {"mv", mv_main, "Move or rename files."},
    {"rm", rm_main, "Remove files."},
    {"chmod", chmod_main, "Change file mode bits."},
    {"chown", chown_main, "Change file ownership."},
    {"whoami", whoami_main, "Print effective username."},
    {"id", id_main, "Print user and group IDs."},
    {"clear", clear_main, "Clear the screen."},
    {"help", help_main, "Show command help."},
    {"time", time_main, "Show time information."},
    {"date", date_main, "Show date information."},
    {"top", top_main, "Show process overview."},
    {"free", free_main, "Show memory usage."},
    {"df", df_main, "Show filesystem usage."},
    {"du", du_main, "Show directory usage."},
    {"ip", ip_main, "Show network interface info."},
    {"ping", ping_main, "Send ICMP echo requests."},
    {"netstat", netstat_main, "Show network sockets."},
    {"route", route_main, "Show routing table."},
    {"ifconfig", ifconfig_main, "Show or configure interfaces."},
    {"login", login_main, "Authenticate a user."},
    {"logout", logout_main, "End the current session."},
    {"useradd", useradd_main, "Create a user account."},
    {"userdel", userdel_main, "Delete a user account."},
    {"groupadd", groupadd_main, "Create a group."},
    {"groupdel", groupdel_main, "Delete a group."},
    {"su", su_main, "Switch user."},
    {"sudo", sudo_main, "Run a command as another user."},
    {"service", service_main, "Manage services."},
    {"start", start_main, "Start a service."},
    {"stop", stop_main, "Stop a service."},
    {"restart", restart_main, "Restart a service."},
    {"status", status_main, "Show service status."},
    {"log", log_main, "Show logs."},
    {"journal", journal_main, "Show journald-like logs."},
    {"sysinfo", sysinfo_main, "Show system summary."},
    {"procinfo", procinfo_main, "Show process details."},
    {"meminfo", meminfo_main, "Show memory details."},
    {"blkinfo", blkinfo_main, "Show block device details."},
    {"devinfo", devinfo_main, "Show device details."},
    {"fdinfo", fdinfo_main, "Show file descriptor info."},
    {"env", env_main, "Print environment."},
    {"export", export_main, "Export environment variables."},
    {"unset", unset_main, "Unset an environment variable."},
    {"set", set_main, "Set shell variables."},
    {"get", get_main, "Get a variable or property."},
    {"alias", alias_main, "Create shell aliases."},
    {"unalias", unalias_main, "Remove shell aliases."},
    {"history", history_main, "Show shell history."},
    {"sleep", sleep_main, "Pause execution."},
    {"watch", watch_main, "Run a command repeatedly."},
    {"find", find_main, "Search files and paths."},
    {"grep", grep_main, "Search for patterns."},
    {"sort", sort_main, "Sort input lines."},
    {"head", head_main, "Print the first lines."},
    {"tail", tail_main, "Print the last lines."},
    {"diff", diff_main, "Compare files."},
    {"patch", patch_main, "Apply patches."},
    {"test", test_main, "Run a tiny self-test."},
    {"benchmark", benchmark_main, "Run a benchmark."},
    {"version", version_main, "Show BlockOS version."},
    {"pkg", pkg_main, "Package manager command."},
    {"update", update_main, "Update package lists."},
}};

const CommandEntry* find_command(std::string_view name) {
    for (const auto& c : kCommands) if (c.name == name) return &c;
    return nullptr;
}

int run_command(std::string_view name, int argc, char** argv) {
    if (const auto* c = find_command(name)) return c->fn(argc, argv);
    std::cerr << "BlockOS: command not found: " << name << '\n';
    return 127;
}

int print_usage(std::string_view name, std::string_view help) {
    std::cout << name << " - " << help << '\n';
    return 0;
}

bool arg_equals(const char* s, std::string_view expected) {
    return s && expected == s;
}

int parse_int(const char* s, int fallback) {
    if (!s) return fallback;
    try { return std::stoi(std::string(s)); } catch (...) { return fallback; }
}

} // namespace blockos::cmd
