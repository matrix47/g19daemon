#include "system_sensors.hpp"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace dashboard {

struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

static QString readFirstLine(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readLine()).trimmed();
}

static QString findCpuTempPath() {
    const QDir hwmonDir(QStringLiteral("/sys/class/hwmon"));
    const QStringList entries = hwmonDir.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs);
    const QStringList preferred = {
        QStringLiteral("coretemp"),
        QStringLiteral("k10temp"),
        QStringLiteral("zenpower"),
        QStringLiteral("acpitz")
    };

    for (const QString &pref : preferred) {
        for (const QString &entry : entries) {
            const QString base = hwmonDir.filePath(entry);
            if (readFirstLine(base + QStringLiteral("/name")) == pref) {
                const QString temp1 = base + QStringLiteral("/temp1_input");
                if (QFile::exists(temp1)) {
                    return temp1;
                }
            }
        }
    }
    return {};
}

SystemSensors::SystemSensors() {
    cpuTempPath_ = findCpuTempPath();
    netTimer_.start();
    QVariantMap warmup;
    sample(warmup);
}

SystemSensors::~SystemSensors() {
    if (nvmlReady_ && fnShutdown_) {
        fnShutdown_();
    }
}

void SystemSensors::initNvml() {
    nvmlAttempted_ = true;
    nvmlLib_.setFileName(QStringLiteral("nvidia-ml"));
    if (!nvmlLib_.load()) {
        nvmlLib_.setFileName(QStringLiteral("libnvidia-ml.so.1"));
        if (!nvmlLib_.load()) {
            return;
        }
    }

    fnInit_ = reinterpret_cast<FnNvmlInit>(nvmlLib_.resolve("nvmlInit_v2"));
    fnShutdown_ = reinterpret_cast<FnNvmlShutdown>(nvmlLib_.resolve("nvmlShutdown"));
    fnGetHandle_ = reinterpret_cast<FnNvmlGetHandle>(nvmlLib_.resolve("nvmlDeviceGetHandleByIndex_v2"));
    fnGetUtil_ = reinterpret_cast<FnNvmlGetUtil>(nvmlLib_.resolve("nvmlDeviceGetUtilizationRates"));
    fnGetMem_ = reinterpret_cast<FnNvmlGetMem>(nvmlLib_.resolve("nvmlDeviceGetMemoryInfo"));
    fnGetTemp_ = reinterpret_cast<FnNvmlGetTemp>(nvmlLib_.resolve("nvmlDeviceGetTemperature"));

    if (fnInit_ && fnGetHandle_ && fnGetUtil_ && fnInit_() == 0) {
        if (fnGetHandle_(0, &nvmlDevice_) == 0 && nvmlDevice_ != nullptr) {
            nvmlReady_ = true;
        }
    }
}

void SystemSensors::sample(QVariantMap &outSnapshot) {
    double dtSec = 1.0;
    if (netTimer_.isValid()) {
        const qint64 ms = netTimer_.restart();
        dtSec = qMax(0.05, ms / 1000.0);
    }

    sampleCpu(outSnapshot);
    sampleMemory(outSnapshot);
    sampleGpu(outSnapshot);
    sampleNetwork(outSnapshot, dtSec);
}

void SystemSensors::sampleCpu(QVariantMap &out) {
    const QString line = readFirstLine(QStringLiteral("/proc/stat"));
    const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    double cpuPct = 0.0;
    if (parts.size() >= 5 && parts[0] == QLatin1String("cpu")) {
        quint64 total = 0;
        for (int i = 1; i < parts.size(); ++i) {
            total += parts[i].toULongLong();
        }
        const quint64 idle = parts[4].toULongLong() + (parts.size() > 5 ? parts[5].toULongLong() : 0);
        const quint64 active = total - idle;

        if (prevCpu_.valid && total > prevCpu_.total) {
            const quint64 dTotal = total - prevCpu_.total;
            const quint64 dActive = active - prevCpu_.active;
            cpuPct = qBound(0.0, (100.0 * dActive) / double(dTotal), 100.0);
        }
        prevCpu_ = {active, total, true};
    }

    out.insert(QStringLiteral("cpu.total"), cpuPct);

    QString detail = QStringLiteral("%1%").arg(qRound(cpuPct));
    if (!cpuTempPath_.isEmpty()) {
        bool ok = false;
        const double milliC = readFirstLine(cpuTempPath_).toDouble(&ok);
        if (ok && milliC > 0) {
            const double tempC = milliC / 1000.0;
            out.insert(QStringLiteral("cpu.temp"), tempC);
            detail += QStringLiteral(" %1°C").arg(qRound(tempC));
        }
    }
    out.insert(QStringLiteral("cpu.detail"), detail);
}

void SystemSensors::sampleMemory(QVariantMap &out) {
    QFile f(QStringLiteral("/proc/meminfo"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    quint64 memTotalKb = 0;
    quint64 memAvailKb = 0;
    quint64 swapTotalKb = 0;
    quint64 swapFreeKb = 0;

    const QString content = QString::fromUtf8(f.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 2) {
            continue;
        }
        if (parts[0] == QLatin1String("MemTotal:")) {
            memTotalKb = parts[1].toULongLong();
        } else if (parts[0] == QLatin1String("MemAvailable:")) {
            memAvailKb = parts[1].toULongLong();
        } else if (parts[0] == QLatin1String("SwapTotal:")) {
            swapTotalKb = parts[1].toULongLong();
        } else if (parts[0] == QLatin1String("SwapFree:")) {
            swapFreeKb = parts[1].toULongLong();
        }
    }

    if (memTotalKb > 0) {
        const quint64 memUsedKb = memTotalKb > memAvailKb ? (memTotalKb - memAvailKb) : 0;
        const double memPct = (100.0 * memUsedKb) / double(memTotalKb);
        const double usedGb = memUsedKb / (1024.0 * 1024.0);
        const double totalGb = memTotalKb / (1024.0 * 1024.0);
        out.insert(QStringLiteral("mem.used_pct"), memPct);
        out.insert(QStringLiteral("mem.used_gb"), usedGb);
        out.insert(QStringLiteral("mem.total_gb"), totalGb);
        out.insert(QStringLiteral("mem.detail"),
                   QStringLiteral("%1/%2G").arg(usedGb, 0, 'f', 1).arg(totalGb, 0, 'f', 0));
    }

    if (swapTotalKb > 0) {
        const quint64 swapUsedKb = swapTotalKb > swapFreeKb ? (swapTotalKb - swapFreeKb) : 0;
        const double swapPct = (100.0 * swapUsedKb) / double(swapTotalKb);
        const double usedGb = swapUsedKb / (1024.0 * 1024.0);
        const double totalGb = swapTotalKb / (1024.0 * 1024.0);
        out.insert(QStringLiteral("swap.used_pct"), swapPct);
        out.insert(QStringLiteral("swap.used_gb"), usedGb);
        out.insert(QStringLiteral("swap.total_gb"), totalGb);
        out.insert(QStringLiteral("swap.detail"),
                   QStringLiteral("%1/%2G").arg(usedGb, 0, 'f', 1).arg(totalGb, 0, 'f', 0));
    } else {
        out.insert(QStringLiteral("swap.used_pct"), 0.0);
        out.insert(QStringLiteral("swap.detail"), QStringLiteral("0G"));
    }
}

void SystemSensors::sampleGpu(QVariantMap &out) {
    if (!nvmlAttempted_) {
        initNvml();
    }

    if (nvmlReady_) {
        NvmlUtilization util{0, 0};
        if (fnGetUtil_(nvmlDevice_, &util) == 0) {
            out.insert(QStringLiteral("gpu.util"), double(util.gpu));
        }

        double tempC = -1.0;
        if (fnGetTemp_) {
            unsigned int t = 0;
            if (fnGetTemp_(nvmlDevice_, 0 /* NVML_TEMPERATURE_GPU */, &t) == 0) {
                tempC = double(t);
                out.insert(QStringLiteral("gpu.temp"), tempC);
            }
        }

        if (fnGetMem_) {
            NvmlMemory mem{0, 0, 0};
            if (fnGetMem_(nvmlDevice_, &mem) == 0 && mem.total > 0) {
                const double pct = (100.0 * mem.used) / double(mem.total);
                out.insert(QStringLiteral("gpu.mem_pct"), pct);
                out.insert(QStringLiteral("gpu.mem_used_gb"), mem.used / (1024.0 * 1024.0 * 1024.0));
                out.insert(QStringLiteral("gpu.mem_total_gb"), mem.total / (1024.0 * 1024.0 * 1024.0));
            }
        }

        QString detail = QStringLiteral("%1%").arg(util.gpu);
        if (tempC >= 0) {
            detail += QStringLiteral(" %1°C").arg(qRound(tempC));
        }
        out.insert(QStringLiteral("gpu.detail"), detail);
        return;
    }

    // Fallback: DRM sysfs (AMD / Intel)
    const QDir drmDir(QStringLiteral("/sys/class/drm"));
    const QStringList cards = drmDir.entryList(QStringList{QStringLiteral("card*")}, QDir::Dirs);
    for (const QString &card : cards) {
        if (card.contains(QLatin1Char('-'))) {
            continue;
        }
        const QString busyPath = drmDir.filePath(card + QStringLiteral("/device/gpu_busy_percent"));
        if (QFile::exists(busyPath)) {
            bool ok = false;
            const double busy = readFirstLine(busyPath).toDouble(&ok);
            if (ok) {
                out.insert(QStringLiteral("gpu.util"), busy);
                out.insert(QStringLiteral("gpu.detail"), QStringLiteral("%1%").arg(qRound(busy)));
                return;
            }
        }
    }

    out.insert(QStringLiteral("gpu.util"), 0.0);
    out.insert(QStringLiteral("gpu.detail"), QStringLiteral("N/A"));
}

void SystemSensors::sampleNetwork(QVariantMap &out, double dtSec) {
    const QDir netDir(QStringLiteral("/sys/class/net"));
    const QStringList ifaces = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &iface : ifaces) {
        if (iface == QLatin1String("lo")) {
            continue;
        }
        const QString base = netDir.filePath(iface);
        const QString state = readFirstLine(base + QStringLiteral("/operstate"));
        const bool isUp = (state == QLatin1String("up"));

        bool rxOk = false;
        bool txOk = false;
        const quint64 rx = readFirstLine(base + QStringLiteral("/statistics/rx_bytes")).toULongLong(&rxOk);
        const quint64 tx = readFirstLine(base + QStringLiteral("/statistics/tx_bytes")).toULongLong(&txOk);
        if (!rxOk || !txOk) {
            continue;
        }

        double rxBps = 0.0;
        double txBps = 0.0;
        if (prevNet_.contains(iface)) {
            const NetCounter &prev = prevNet_[iface];
            if (prev.valid && rx >= prev.rxBytes && tx >= prev.txBytes) {
                rxBps = double(rx - prev.rxBytes) / dtSec;
                txBps = double(tx - prev.txBytes) / dtSec;
            }
        }
        prevNet_.insert(iface, {rx, tx, true});

        bool speedOk = false;
        const double speedMbps = readFirstLine(base + QStringLiteral("/speed")).toDouble(&speedOk);

        const QString prefix = QStringLiteral("net.") + iface + QLatin1Char('.');
        out.insert(prefix + QStringLiteral("up"), isUp);
        out.insert(prefix + QStringLiteral("rx_bps"), rxBps);
        out.insert(prefix + QStringLiteral("tx_bps"), txBps);
        if (speedOk && speedMbps > 0) {
            out.insert(prefix + QStringLiteral("speed_mbps"), speedMbps);
        }
    }
}

}  // namespace dashboard
