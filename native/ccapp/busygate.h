// busygate.h - one broker action at a time in the Control Center (review A26). The plain page (ccapp.cpp) and the
// Access look (skinpage.cpp) had a busy flag each, so a backup in the Access look and the app's restore (its RESTORE
// button) could run at once on two broker connections. Both take this gate; a refused request says so (A33).
#pragma once
#include <atomic>

namespace tirevive {

inline std::atomic<bool> &cc_busy()
{
    static std::atomic<bool> busy{false};
    return busy;
}
constexpr const char *kBusyNote = "Busy: another action is still running"; // fits the Access look's footer (36+)

} // namespace tirevive
