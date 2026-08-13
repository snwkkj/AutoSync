#pragma once

struct SystemMetrics
{
    int cpuUsagePercent = -1;
    int memoryUsagePercent = -1;
    int cpuTemperatureCelsius = -1;
};

class SystemMonitor
{
public:
    SystemMetrics sample();

private:
    unsigned long long previousCpuIdle_ = 0;
    unsigned long long previousCpuTotal_ = 0;
};
