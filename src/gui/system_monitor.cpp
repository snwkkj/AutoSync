#include "system_monitor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int readMemoryUsage()
{
    std::ifstream information("/proc/meminfo");
    std::string key;
    unsigned long long value = 0;
    std::string unit;
    unsigned long long total = 0;
    unsigned long long available = 0;

    while (information >> key >> value >> unit) {
        if (key == "MemTotal:") {
            total = value;
        } else if (key == "MemAvailable:") {
            available = value;
        }
        if (total > 0 && available > 0) {
            break;
        }
    }
    if (total == 0 || available > total) {
        return -1;
    }
    return static_cast<int>(std::lround(
        100.0 * static_cast<double>(total - available) / static_cast<double>(total)));
}

double readCpuTemperature()
{
    namespace fs = std::filesystem;
    const fs::path hwmonRoot("/sys/class/hwmon");
    std::vector<double> fallback;
    std::error_code error;

    if (fs::exists(hwmonRoot, error)) {
        for (const auto &directory : fs::directory_iterator(hwmonRoot, error)) {
            if (error) {
                break;
            }
            for (const auto &entry : fs::directory_iterator(directory.path(), error)) {
                const std::string name = entry.path().filename().string();
                if (name.rfind("temp", 0) != 0 ||
                    name.find("_input") == std::string::npos) {
                    continue;
                }
                std::ifstream input(entry.path());
                double millidegrees = 0.0;
                if (!(input >> millidegrees) || millidegrees <= 0.0 ||
                    millidegrees > 150000.0) {
                    continue;
                }
                const double value = millidegrees / 1000.0;
                const std::string stem = name.substr(0, name.find("_input"));
                std::ifstream labelFile(directory.path() / (stem + "_label"));
                std::string label;
                std::getline(labelFile, label);
                std::transform(label.begin(), label.end(), label.begin(),
                               [](unsigned char character) {
                                   return static_cast<char>(std::tolower(character));
                               });
                if (label.find("package") != std::string::npos ||
                    label.find("tctl") != std::string::npos ||
                    label.find("cpu") != std::string::npos) {
                    return value;
                }
                fallback.push_back(value);
            }
        }
    }

    const fs::path thermalRoot("/sys/class/thermal");
    if (fs::exists(thermalRoot, error)) {
        for (const auto &directory : fs::directory_iterator(thermalRoot, error)) {
            if (error ||
                directory.path().filename().string().rfind("thermal_zone", 0) != 0) {
                continue;
            }
            std::ifstream input(directory.path() / "temp");
            double millidegrees = 0.0;
            if (input >> millidegrees && millidegrees > 0.0 &&
                millidegrees <= 150000.0) {
                fallback.push_back(millidegrees / 1000.0);
            }
        }
    }
    return fallback.empty()
               ? -1.0
               : *std::max_element(fallback.begin(), fallback.end());
}

} // namespace

SystemMetrics SystemMonitor::sample()
{
    SystemMetrics metrics;
    metrics.memoryUsagePercent = readMemoryUsage();
    const double temperature = readCpuTemperature();
    if (temperature >= 0.0) {
        metrics.cpuTemperatureCelsius = static_cast<int>(std::lround(temperature));
    }

    std::ifstream statistics("/proc/stat");
    std::string line;
    std::string label;
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long ioWait = 0;
    unsigned long long irq = 0;
    unsigned long long softIrq = 0;
    unsigned long long steal = 0;

    if (!std::getline(statistics, line)) {
        return metrics;
    }
    std::istringstream values(line);
    values >> label >> user >> nice >> system >> idle >> ioWait >> irq >> softIrq >> steal;
    if (label != "cpu") {
        return metrics;
    }

    const unsigned long long idleTotal = idle + ioWait;
    const unsigned long long total =
        user + nice + system + idle + ioWait + irq + softIrq + steal;
    if (previousCpuTotal_ != 0 && total > previousCpuTotal_) {
        const unsigned long long totalDelta = total - previousCpuTotal_;
        const unsigned long long idleDelta = idleTotal - previousCpuIdle_;
        const double usage =
            100.0 * static_cast<double>(totalDelta - std::min(idleDelta, totalDelta)) /
            static_cast<double>(totalDelta);
        metrics.cpuUsagePercent = static_cast<int>(std::lround(usage));
    }
    previousCpuIdle_ = idleTotal;
    previousCpuTotal_ = total;
    return metrics;
}
