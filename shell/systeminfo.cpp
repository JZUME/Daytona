// SPDX-License-Identifier: GPL-3.0-or-later

#include "systeminfo.h"

#include "systemlogo.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSet>
#include <QSysInfo>
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace Daytona {

namespace {

    QByteArray readFile(const QString &path, qint64 limit = 1024 * 1024)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.read(limit);
    }

    // "key<tabs> : value" lines, as /proc/cpuinfo and /proc/meminfo have.
    QPair<QString, QString> procField(const QByteArray &line)
    {
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            return {};
        return { QString::fromUtf8(line.left(colon)).trimmed(),
            QString::fromUtf8(line.mid(colon + 1)).trimmed() };
    }

    // ARM's CPU implementer codes (the first byte of MIDR), as lscpu
    // names them.
    QString armImplementer(int code)
    {
        switch (code) {
        case 0x41:
            return QStringLiteral("ARM");
        case 0x42:
            return QStringLiteral("Broadcom");
        case 0x43:
            return QStringLiteral("Cavium");
        case 0x46:
            return QStringLiteral("Fujitsu");
        case 0x48:
            return QStringLiteral("HiSilicon");
        case 0x4e:
            return QStringLiteral("NVIDIA");
        case 0x51:
            return QStringLiteral("Qualcomm");
        case 0x53:
            return QStringLiteral("Samsung");
        case 0x61:
            return QStringLiteral("Apple");
        case 0xc0:
            return QStringLiteral("Ampere");
        default:
            return {};
        }
    }

    // Arm Ltd's own cores (implementer 0x41), the common ones.
    QString armCore(int part)
    {
        static const QHash<int, const char *> cores = {
            { 0xb76, "ARM1176" },
            { 0xc07, "Cortex-A7" },
            { 0xc08, "Cortex-A8" },
            { 0xc09, "Cortex-A9" },
            { 0xc0f, "Cortex-A15" },
            { 0xc0e, "Cortex-A17" },
            { 0xd01, "Cortex-A32" },
            { 0xd03, "Cortex-A53" },
            { 0xd04, "Cortex-A35" },
            { 0xd05, "Cortex-A55" },
            { 0xd07, "Cortex-A57" },
            { 0xd08, "Cortex-A72" },
            { 0xd09, "Cortex-A73" },
            { 0xd0a, "Cortex-A75" },
            { 0xd0b, "Cortex-A76" },
            { 0xd0c, "Neoverse-N1" },
            { 0xd0d, "Cortex-A77" },
            { 0xd40, "Neoverse-V1" },
            { 0xd41, "Cortex-A78" },
            { 0xd44, "Cortex-X1" },
            { 0xd46, "Cortex-A510" },
            { 0xd47, "Cortex-A710" },
            { 0xd48, "Cortex-X2" },
            { 0xd49, "Neoverse-N2" },
            { 0xd4b, "Cortex-A78C" },
            { 0xd4d, "Cortex-A715" },
            { 0xd4e, "Cortex-X3" },
            { 0xd4f, "Neoverse-V2" },
            { 0xd80, "Cortex-A520" },
            { 0xd81, "Cortex-A720" },
            { 0xd82, "Cortex-X4" },
        };
        const char *name = cores.value(part);
        return name ? QString::fromLatin1(name) : QString();
    }

    int hexValue(const QString &text)
    {
        bool ok = false;
        const int value = text.toInt(&ok, 0);
        return ok ? value : -1;
    }

} // namespace

QString operatingSystemName(const QString &osRelease)
{
    const QHash<QString, QString> fields = parseOsRelease(osRelease);
    const QString pretty = fields.value(QStringLiteral("PRETTY_NAME"));
    if (!pretty.isEmpty())
        return pretty;
    const QString name = fields.value(QStringLiteral("NAME"), QStringLiteral("Linux"));
    QString version = fields.value(QStringLiteral("VERSION"));
    if (version.isEmpty())
        version = fields.value(QStringLiteral("VERSION_ID"));
    return version.isEmpty() ? name : name + QLatin1Char(' ') + version;
}

ProcessorInfo parseCpuInfo(
    const QByteArray &cpuinfo, const QString &boardModel, const QString &machine)
{
    ProcessorInfo info;
    QString modelName, hardware, armModel, uarch, cpu;
    QStringList armCores; // in order, each once
    int implementer = -1;
    for (const QByteArray &line : cpuinfo.split('\n')) {
        const auto [key, value] = procField(line);
        if (key.isEmpty())
            continue;
        if (key == QLatin1String("processor"))
            ++info.count;
        else if (key == QLatin1String("vendor_id") && info.vendor.isEmpty())
            info.vendor = value;
        else if ((key == QLatin1String("model name") || key == QLatin1String("cpu model"))
            && modelName.isEmpty())
            modelName = value;
        else if (key == QLatin1String("CPU implementer"))
            implementer = hexValue(value);
        else if (key == QLatin1String("CPU part") && implementer == 0x41) {
            const QString core = armCore(hexValue(value));
            if (!core.isEmpty() && !armCores.contains(core))
                armCores.append(core);
        } else if (key == QLatin1String("Hardware") && hardware.isEmpty())
            hardware = value;
        else if (key == QLatin1String("Model") && armModel.isEmpty())
            armModel = value;
        else if (key == QLatin1String("uarch") && uarch.isEmpty())
            uarch = value;
        else if (key == QLatin1String("cpu") && cpu.isEmpty())
            cpu = value;
    }
    if (info.vendor.isEmpty() && implementer >= 0)
        info.vendor = armImplementer(implementer);

    // Older ARM kernels put a description in "model name" ("ARMv7
    // Processor rev 3 (v7l)"); the core's name says more.
    if (!armCores.isEmpty())
        info.model = armCores.join(QStringLiteral(" + "));
    else if (!modelName.isEmpty())
        info.model = modelName;
    else if (!uarch.isEmpty())
        info.model = uarch;
    else if (!cpu.isEmpty())
        info.model = cpu;
    else if (!hardware.isEmpty())
        info.model = hardware;
    else if (!armModel.isEmpty())
        info.model = armModel;
    else if (!boardModel.isEmpty())
        info.model = boardModel;
    else
        info.model = machine;
    return info;
}

MemoryInfo parseMemInfo(const QByteArray &meminfo)
{
    MemoryInfo info;
    qint64 free = 0, buffers = 0, cached = 0;
    bool hasAvailable = false;
    for (const QByteArray &line : meminfo.split('\n')) {
        const auto [key, value] = procField(line);
        if (key.isEmpty())
            continue;
        // "12345 kB"
        const qint64 kib = value.section(QLatin1Char(' '), 0, 0).toLongLong();
        if (key == QLatin1String("MemTotal"))
            info.total = kib;
        else if (key == QLatin1String("MemAvailable")) {
            info.available = kib;
            hasAvailable = true;
        } else if (key == QLatin1String("MemFree"))
            free = kib;
        else if (key == QLatin1String("Buffers"))
            buffers = kib;
        else if (key == QLatin1String("Cached"))
            cached = kib;
        else if (key == QLatin1String("SwapTotal"))
            info.swapTotal = kib;
        else if (key == QLatin1String("SwapFree"))
            info.swapFree = kib;
    }
    // Kernels before 3.14 have no MemAvailable.
    if (!hasAvailable)
        info.available = qMin(info.total, free + buffers + cached);
    return info;
}

QList<SwapArea> parseSwaps(const QByteArray &swaps)
{
    // "Filename  Type  Size  Used  Priority", sizes in KiB. Names with
    // spaces are written with \040.
    QList<SwapArea> areas;
    const QList<QByteArray> lines = swaps.split('\n');
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = lines.at(i).simplified().split(' ');
        if (fields.size() < 4)
            continue;
        SwapArea area;
        area.name = QString::fromUtf8(QByteArray(fields.at(0)).replace("\\040", " "));
        area.type = QString::fromUtf8(fields.at(1));
        area.size = fields.at(2).toLongLong();
        area.used = fields.at(3).toLongLong();
        areas.append(area);
    }
    return areas;
}

QString memorySize(qint64 kib)
{
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("Daytona::SystemPropertiesDialog", text);
    };
    const QLocale locale;
    constexpr qint64 Mib = 1024;
    constexpr qint64 Gib = 1024 * 1024;
    if (kib < Gib)
        return tr("%1 MB").arg(locale.toString((kib + Mib / 2) / Mib));
    return tr("%1 GB").arg(locale.toString(double(kib) / Gib, 'f', 1));
}

QString computerName(const QString &machineInfo, const QString &hostname)
{
    const QString pretty = parseOsRelease(machineInfo).value(QStringLiteral("PRETTY_HOSTNAME"));
    return pretty.isEmpty() ? hostname : pretty;
}

QString fullUserName(const QString &gecos, const QString &login)
{
    const QString name = gecos.section(QLatin1Char(','), 0, 0).trimmed();
    return name.isEmpty() ? login : name;
}

SystemInfo readSystemInfo(const QString &root)
{
    SystemInfo info;

    QByteArray osRelease = readFile(root + QStringLiteral("/etc/os-release"));
    if (osRelease.isEmpty())
        osRelease = readFile(root + QStringLiteral("/usr/lib/os-release"));
    info.operatingSystem = operatingSystemName(QString::fromUtf8(osRelease));
    info.daytonaVersion = QStringLiteral(DAYTONA_VERSION);

    struct utsname uts = {};
    QString machine = QSysInfo::currentCpuArchitecture();
    if (uname(&uts) == 0) {
        info.kernel = QString::fromLocal8Bit(uts.sysname) + QLatin1Char(' ')
            + QString::fromLocal8Bit(uts.release);
        machine = QString::fromLocal8Bit(uts.machine);
    }

    if (const struct passwd *pw = getpwuid(getuid()))
        info.userName = fullUserName(
            QString::fromLocal8Bit(pw->pw_gecos), QString::fromLocal8Bit(pw->pw_name));
    info.computerName
        = computerName(QString::fromUtf8(readFile(root + QStringLiteral("/etc/machine-info"))),
            QSysInfo::machineHostName());

    // The board, for processors cpuinfo does not name: the devicetree's
    // model (ARM, RISC-V), else the DMI product name.
    QString board = QString::fromUtf8(
        readFile(root + QStringLiteral("/sys/firmware/devicetree/base/model"), 4096))
                        .remove(QChar(0))
                        .trimmed();
    if (board.isEmpty())
        board = QString::fromUtf8(
            readFile(root + QStringLiteral("/sys/devices/virtual/dmi/id/product_name"), 4096))
                    .trimmed();
    info.processor = parseCpuInfo(
        readFile(root + QStringLiteral("/proc/cpuinfo"), 4 * 1024 * 1024), board, machine);
    info.memory = parseMemInfo(readFile(root + QStringLiteral("/proc/meminfo")));
    info.swapAreas = parseSwaps(readFile(root + QStringLiteral("/proc/swaps")));
    info.logo = systemLogo(root);
    return info;
}

} // namespace Daytona
