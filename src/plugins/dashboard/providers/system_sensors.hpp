#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QLibrary>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>

namespace dashboard {

class SystemSensors {
public:
    SystemSensors();
    ~SystemSensors();

    void sample(QVariantMap &outSnapshot);

private:
    void sampleCpu(QVariantMap &out);
    void sampleMemory(QVariantMap &out);
    void sampleGpu(QVariantMap &out);
    void sampleNetwork(QVariantMap &out, double dtSec);
    void initNvml();

    struct CpuTimes {
        quint64 active = 0;
        quint64 total = 0;
        bool valid = false;
    };
    CpuTimes prevCpu_;
    QString cpuTempPath_;

    struct NetCounter {
        quint64 rxBytes = 0;
        quint64 txBytes = 0;
        bool valid = false;
    };
    QHash<QString, NetCounter> prevNet_;
    QElapsedTimer netTimer_;

    // Optional NVML via QLibrary (zero-fork NVIDIA telemetry)
    QLibrary nvmlLib_;
    bool nvmlAttempted_ = false;
    bool nvmlReady_ = false;
    void *nvmlDevice_ = nullptr;

    using FnNvmlInit = int (*)();
    using FnNvmlShutdown = int (*)();
    using FnNvmlGetHandle = int (*)(unsigned int, void **);
    using FnNvmlGetUtil = int (*)(void *, void *);
    using FnNvmlGetMem = int (*)(void *, void *);
    using FnNvmlGetTemp = int (*)(void *, int, unsigned int *);

    FnNvmlInit fnInit_ = nullptr;
    FnNvmlShutdown fnShutdown_ = nullptr;
    FnNvmlGetHandle fnGetHandle_ = nullptr;
    FnNvmlGetUtil fnGetUtil_ = nullptr;
    FnNvmlGetMem fnGetMem_ = nullptr;
    FnNvmlGetTemp fnGetTemp_ = nullptr;
};

}  // namespace dashboard
