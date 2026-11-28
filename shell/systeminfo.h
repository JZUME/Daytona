// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace Daytona {

// What System Properties shows, read from /etc/os-release, /proc and a few
// other files that need no root. The parsers take the files' contents so
// they can be tested with sample files.

// The distribution's name from os-release: PRETTY_NAME, else NAME and
// VERSION (or VERSION_ID), else "Linux".
QString operatingSystemName(const QString &osRelease);

// The processor, from /proc/cpuinfo.
struct ProcessorInfo
{
    QString vendor; // "GenuineIntel", "ARM"; empty if unknown
    QString model; // "Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz", "Cortex-A72"
    int count = 0; // logical processors; 0 if cpuinfo lists none
};

// x86 has vendor_id and model name. ARM has CPU implementer and CPU part
// (named from a table of the common ones, both parts of a big.LITTLE
// processor), and sometimes Hardware or Model; RISC-V has uarch, POWER
// has cpu. Where cpuinfo names no model, boardModel (the devicetree or
// DMI name) is used, else machine (uname -m).
ProcessorInfo parseCpuInfo(
    const QByteArray &cpuinfo, const QString &boardModel = {}, const QString &machine = {});

// Memory and swap from /proc/meminfo, in KiB.
struct MemoryInfo
{
    qint64 total = 0; // MemTotal
    qint64 available = 0; // MemAvailable, else MemFree + Buffers + Cached
    qint64 swapTotal = 0;
    qint64 swapFree = 0;

    qint64 used() const { return qMax<qint64>(0, total - available); }
    qint64 swapUsed() const { return qMax<qint64>(0, swapTotal - swapFree); }
};
MemoryInfo parseMemInfo(const QByteArray &meminfo);

// One swap device or file from /proc/swaps (zram included), sizes in KiB.
struct SwapArea
{
    QString name; // "/dev/zram0", "/swapfile"
    QString type; // "partition", "file"
    qint64 size = 0;
    qint64 used = 0;
};
QList<SwapArea> parseSwaps(const QByteArray &swaps);

// A size in KiB as Windows wrote memory: "15.5 GB" (one decimal, 1024-based
// as Windows counted), or "512 MB" under 1 GB.
QString memorySize(qint64 kib);

// The computer's name: PRETTY_HOSTNAME from /etc/machine-info if set, else
// hostname.
QString computerName(const QString &machineInfo, const QString &hostname);

// The user's full name from the GECOS field (up to the first comma), else
// the login name.
QString fullUserName(const QString &gecos, const QString &login);

// Everything System Properties shows, read once when it opens.
struct SystemInfo
{
    QString operatingSystem;
    QString daytonaVersion;
    QString kernel; // "Linux 6.12.48-amd64"
    QString userName;
    QString computerName;
    ProcessorInfo processor;
    MemoryInfo memory;
    QList<SwapArea> swapAreas;
};

// Reads this computer's files. root prefixes every path (tests).
SystemInfo readSystemInfo(const QString &root = {});

} // namespace Daytona
