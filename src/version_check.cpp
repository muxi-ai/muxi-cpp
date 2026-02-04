#include "muxi/version_check.hpp"
#include "muxi/version.hpp"
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <atomic>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace muxi {
namespace internal {

static std::atomic<bool> checked{false};
static constexpr int64_t TWELVE_HOURS_SECS = 12 * 60 * 60;
static const std::string SDK_NAME = "cpp";

static bool notifications_disabled() {
    const char* val = std::getenv("MUXI_SDK_VERSION_NOTIFICATION");
    return val && std::string(val) == "0";
}

static std::string get_home_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return path;
    }
    return "";
#else
    const char* home = std::getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "";
#endif
}

static std::string get_cache_path() {
    std::string home = get_home_dir();
    if (home.empty()) return "";
    return home + "/.muxi/sdk-versions.json";
}

static void ensure_dir_exists(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return;
    std::string dir = path.substr(0, pos);
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
}

static nlohmann::json load_cache() {
    std::string path = get_cache_path();
    if (path.empty()) return nlohmann::json::object();
    
    std::ifstream file(path);
    if (!file.is_open()) return nlohmann::json::object();
    
    try {
        nlohmann::json cache;
        file >> cache;
        return cache;
    } catch (...) {
        return nlohmann::json::object();
    }
}

static void save_cache(const nlohmann::json& cache) {
    std::string path = get_cache_path();
    if (path.empty()) return;
    
    ensure_dir_exists(path);
    std::ofstream file(path);
    if (file.is_open()) {
        file << cache.dump(2);
    }
}

static bool is_newer_version(const std::string& latest, const std::string& current) {
    return latest > current;
}

static int64_t now_secs() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

static bool notified_recently() {
    auto cache = load_cache();
    if (!cache.contains(SDK_NAME)) return false;
    auto& entry = cache[SDK_NAME];
    if (!entry.contains("last_notified")) return false;
    
    int64_t last_notified = entry["last_notified"].get<int64_t>();
    return (now_secs() - last_notified) < TWELVE_HOURS_SECS;
}

static void update_latest_version(const std::string& latest) {
    auto cache = load_cache();
    cache[SDK_NAME]["current"] = VERSION;
    cache[SDK_NAME]["latest"] = latest;
    save_cache(cache);
}

static void mark_notified() {
    auto cache = load_cache();
    if (cache.contains(SDK_NAME)) {
        cache[SDK_NAME]["last_notified"] = now_secs();
        save_cache(cache);
    }
}

void check_for_updates(const std::unordered_map<std::string, std::string>& headers) {
    bool expected = false;
    if (!checked.compare_exchange_strong(expected, true)) return;
    if (notifications_disabled()) return;
    
    std::string latest;
    auto it = headers.find("X-Muxi-SDK-Latest");
    if (it == headers.end()) {
        it = headers.find("x-muxi-sdk-latest");
    }
    if (it == headers.end()) return;
    latest = it->second;
    
    if (!is_newer_version(latest, VERSION)) return;
    
    update_latest_version(latest);
    
    if (!notified_recently()) {
        std::cerr << "[muxi] SDK update available: " << latest << " (current: " << VERSION << ")" << std::endl;
        std::cerr << "[muxi] Update your muxi-cpp dependency to " << latest << std::endl;
        mark_notified();
    }
}

} // namespace internal
} // namespace muxi
