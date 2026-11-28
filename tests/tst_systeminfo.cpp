// SPDX-License-Identifier: GPL-3.0-or-later

#include "systeminfo.h"
#include "systemproperties.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QProgressBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace Daytona;

namespace {

const char X86CpuInfo[] = R"(processor	: 0
vendor_id	: GenuineIntel
cpu family	: 6
model		: 142
model name	: Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz
flags		: fpu vme de pse

processor	: 1
vendor_id	: GenuineIntel
cpu family	: 6
model		: 142
model name	: Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz
flags		: fpu vme de pse
)";

// A Raspberry Pi 4 (64-bit kernel).
const char Pi4CpuInfo[] = R"(processor	: 0
BogoMIPS	: 108.00
Features	: fp asimd evtstrm crc32 cpuid
CPU implementer	: 0x41
CPU architecture: 8
CPU variant	: 0x0
CPU part	: 0xd08
CPU revision	: 3

processor	: 1
BogoMIPS	: 108.00
CPU implementer	: 0x41
CPU part	: 0xd08

processor	: 2
CPU implementer	: 0x41
CPU part	: 0xd08

processor	: 3
CPU implementer	: 0x41
CPU part	: 0xd08

Revision	: c03114
Serial		: 100000001234abcd
Model		: Raspberry Pi 4 Model B Rev 1.4
)";

// A big.LITTLE phone-class processor: little cores listed first.
const char BigLittleCpuInfo[] = R"(processor	: 0
CPU implementer	: 0x41
CPU part	: 0xd05
processor	: 1
CPU implementer	: 0x41
CPU part	: 0xd05
processor	: 2
CPU implementer	: 0x41
CPU part	: 0xd0b
processor	: 3
CPU implementer	: 0x41
CPU part	: 0xd0b
)";

// A 32-bit kernel: "model name" is only a description.
const char Armv7CpuInfo[] = R"(processor	: 0
model name	: ARMv7 Processor rev 4 (v7l)
BogoMIPS	: 38.40
CPU implementer	: 0x41
CPU part	: 0xc07
Hardware	: BCM2835
)";

// A core cpuinfo has no name for: Hardware says what it is.
const char UnknownArmCpuInfo[] = R"(processor	: 0
CPU implementer	: 0x51
CPU part	: 0x801
Hardware	: Qualcomm Technologies, Inc SM8150
)";

const char RiscvCpuInfo[] = R"(processor	: 0
hart		: 0
isa		: rv64imafdc
mmu		: sv39
uarch		: sifive,u74-mc

processor	: 1
hart		: 1
isa		: rv64imafdc
uarch		: sifive,u74-mc
)";

const char MemInfo[] = R"(MemTotal:       16251280 kB
MemFree:         8000000 kB
MemAvailable:   12188460 kB
Buffers:          300000 kB
Cached:          3000000 kB
SwapCached:            0 kB
SwapTotal:       4194300 kB
SwapFree:        3145725 kB
)";

const char Swaps[]
    = R"(Filename				Type		Size		Used		Priority
/dev/zram0                              partition	4194300		1048575		100
/swap\040file                           file		2097148		0		-2
)";

QString gb(double value)
{
    return QLocale().toString(value, 'f', 1) + QStringLiteral(" GB");
}

} // namespace

class TestSystemInfo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void operatingSystem()
    {
        QCOMPARE(operatingSystemName(QStringLiteral(
                     "PRETTY_NAME=\"Debian GNU/Linux 13 (trixie)\"\nNAME=\"Debian GNU/Linux\"\n"
                     "VERSION_ID=\"13\"\nVERSION=\"13 (trixie)\"\n")),
            QStringLiteral("Debian GNU/Linux 13 (trixie)"));
        QCOMPARE(operatingSystemName(QStringLiteral("NAME=Fedora\nVERSION='41 (Workstation)'\n")),
            QStringLiteral("Fedora 41 (Workstation)"));
        QCOMPARE(operatingSystemName(QStringLiteral("NAME=Arch\nVERSION_ID=rolling\n")),
            QStringLiteral("Arch rolling"));
        QCOMPARE(operatingSystemName(QStringLiteral("NAME=\"Arch Linux\"\n")),
            QStringLiteral("Arch Linux"));
        QCOMPARE(operatingSystemName(QString()), QStringLiteral("Linux"));
    }

    void x86()
    {
        const ProcessorInfo cpu = parseCpuInfo(X86CpuInfo, QStringLiteral("Board"));
        QCOMPARE(cpu.vendor, QStringLiteral("GenuineIntel"));
        QCOMPARE(cpu.model, QStringLiteral("Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"));
        QCOMPARE(cpu.count, 2);
    }

    void arm()
    {
        ProcessorInfo cpu = parseCpuInfo(Pi4CpuInfo, QStringLiteral("Raspberry Pi 4"));
        QCOMPARE(cpu.vendor, QStringLiteral("ARM"));
        QCOMPARE(cpu.model, QStringLiteral("Cortex-A72"));
        QCOMPARE(cpu.count, 4);

        cpu = parseCpuInfo(BigLittleCpuInfo);
        QCOMPARE(cpu.model, QStringLiteral("Cortex-A55 + Cortex-A76"));
        QCOMPARE(cpu.count, 4);

        // The core's name rather than the kernel's description.
        cpu = parseCpuInfo(Armv7CpuInfo);
        QCOMPARE(cpu.model, QStringLiteral("Cortex-A7"));
        QCOMPARE(cpu.count, 1);

        cpu = parseCpuInfo(UnknownArmCpuInfo);
        QCOMPARE(cpu.vendor, QStringLiteral("Qualcomm"));
        QCOMPARE(cpu.model, QStringLiteral("Qualcomm Technologies, Inc SM8150"));
    }

    void otherArchitectures()
    {
        ProcessorInfo cpu = parseCpuInfo(RiscvCpuInfo);
        QCOMPARE(cpu.vendor, QString());
        QCOMPARE(cpu.model, QStringLiteral("sifive,u74-mc"));
        QCOMPARE(cpu.count, 2);

        cpu = parseCpuInfo("processor\t: 0\ncpu\t\t: POWER9 (raw), altivec supported\n");
        QCOMPARE(cpu.model, QStringLiteral("POWER9 (raw), altivec supported"));

        // Nothing in cpuinfo: the board, then the machine type.
        cpu = parseCpuInfo(
            "processor\t: 0\n", QStringLiteral("StarFive VisionFive 2"), QStringLiteral("riscv64"));
        QCOMPARE(cpu.model, QStringLiteral("StarFive VisionFive 2"));
        cpu = parseCpuInfo(QByteArray(), QString(), QStringLiteral("aarch64"));
        QCOMPARE(cpu.model, QStringLiteral("aarch64"));
        QCOMPARE(cpu.count, 0);
    }

    void memory()
    {
        const MemoryInfo memory = parseMemInfo(MemInfo);
        QCOMPARE(memory.total, 16251280);
        QCOMPARE(memory.available, 12188460);
        QCOMPARE(memory.used(), 16251280 - 12188460);
        QCOMPARE(memory.swapTotal, 4194300);
        QCOMPARE(memory.swapUsed(), 4194300 - 3145725);

        // No MemAvailable (before Linux 3.14): free, buffers and cache.
        const MemoryInfo old
            = parseMemInfo("MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 250 kB\n");
        QCOMPARE(old.available, 400);
        QCOMPARE(old.swapTotal, 0);
        QCOMPARE(old.swapUsed(), 0);
    }

    void swaps()
    {
        const QList<SwapArea> areas = parseSwaps(Swaps);
        QCOMPARE(areas.size(), 2);
        QCOMPARE(areas.at(0).name, QStringLiteral("/dev/zram0"));
        QCOMPARE(areas.at(0).type, QStringLiteral("partition"));
        QCOMPARE(areas.at(0).size, 4194300);
        QCOMPARE(areas.at(0).used, 1048575);
        QCOMPARE(areas.at(1).name, QStringLiteral("/swap file"));
        QCOMPARE(areas.at(1).type, QStringLiteral("file"));
        // Only the heading: no swap.
        QVERIFY(parseSwaps("Filename\tType\tSize\tUsed\tPriority\n").isEmpty());
    }

    void sizes()
    {
        QCOMPARE(memorySize(16251280), gb(15.5));
        QCOMPARE(memorySize(1024 * 1024), gb(1.0));
        QCOMPARE(memorySize(512 * 1024), QStringLiteral("512 MB"));
        QCOMPARE(memorySize(0), QStringLiteral("0 MB"));
    }

    void names()
    {
        QCOMPARE(computerName(QStringLiteral("PRETTY_HOSTNAME=\"Casey's Laptop\"\n"),
                     QStringLiteral("laptop")),
            QStringLiteral("Casey's Laptop"));
        QCOMPARE(computerName(QString(), QStringLiteral("laptop")), QStringLiteral("laptop"));
        QCOMPARE(fullUserName(QStringLiteral("Casey Jones,,,"), QStringLiteral("casey")),
            QStringLiteral("Casey Jones"));
        QCOMPARE(
            fullUserName(QStringLiteral(",,,"), QStringLiteral("casey")), QStringLiteral("casey"));
    }

    void readsFiles()
    {
        // A root with sample files in place of the system's.
        QTemporaryDir root;
        const auto write = [&root](const QString &path, const QByteArray &data) {
            QDir().mkpath(QFileInfo(root.path() + path).path());
            QFile file(root.path() + path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(data);
        };
        write(QStringLiteral("/usr/lib/os-release"), "PRETTY_NAME=\"Debian GNU/Linux 13\"\n");
        write(QStringLiteral("/proc/cpuinfo"), X86CpuInfo);
        write(QStringLiteral("/proc/meminfo"), MemInfo);
        write(QStringLiteral("/proc/swaps"), Swaps);
        const SystemInfo info = readSystemInfo(root.path());
        QCOMPARE(info.operatingSystem, QStringLiteral("Debian GNU/Linux 13"));
        QCOMPARE(info.daytonaVersion, QStringLiteral(DAYTONA_VERSION));
        QVERIFY(info.kernel.startsWith(QLatin1String("Linux ")));
        QVERIFY(!info.userName.isEmpty());
        QVERIFY(!info.computerName.isEmpty());
        QCOMPARE(info.processor.count, 2);
        QCOMPARE(info.memory.total, 16251280);
        QCOMPARE(info.swapAreas.size(), 2);
        QVERIFY(info.logo.isNull());
    }

    void readsThisComputer()
    {
        // The real files, whatever this machine is.
        const SystemInfo info = readSystemInfo();
        QVERIFY(!info.operatingSystem.isEmpty());
        QVERIFY(!info.processor.model.isEmpty());
        QVERIFY(info.processor.count > 0);
        QVERIFY(info.memory.total > 0);
    }

    void dialog()
    {
        SystemPropertiesDialog dialog;
        dialog.setReader([] {
            SystemInfo info;
            info.operatingSystem = QStringLiteral("Debian GNU/Linux 13 (trixie)");
            info.daytonaVersion = QStringLiteral("0.2.0");
            info.kernel = QStringLiteral("Linux 6.12.48-amd64");
            info.userName = QStringLiteral("Casey");
            info.computerName = QStringLiteral("daytona");
            info.processor = parseCpuInfo(X86CpuInfo);
            info.memory = parseMemInfo(MemInfo);
            info.swapAreas = parseSwaps(Swaps);
            QPixmap logo(16, 16);
            logo.fill(Qt::red);
            info.logo = QIcon(logo);
            return info;
        });
        dialog.prepare();
        QCOMPARE(dialog.windowTitle(), QStringLiteral("System Properties"));
        QCOMPARE(dialog.tabs()->count(), 2);
        // The distribution's logo under the computer.
        QVERIFY(!dialog.logo()->isHidden());
        QVERIFY(!dialog.logo()->pixmap().isNull());
        QCOMPARE(dialog.tabs()->tabText(0), QStringLiteral("General"));
        QCOMPARE(dialog.tabs()->tabText(1), QStringLiteral("Performance"));
        QCOMPARE(dialog.generalText(),
            QStringList({ QStringLiteral("System:"), QStringLiteral("Debian GNU/Linux 13 (trixie)"),
                QStringLiteral("Daytona 0.2.0"), QStringLiteral("Linux 6.12.48-amd64"),
                QStringLiteral("Registered to:"), QStringLiteral("Casey"),
                QStringLiteral("Computer name:"), QStringLiteral("daytona"),
                QStringLiteral("Computer:"), QStringLiteral("GenuineIntel"),
                QStringLiteral("Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"),
                QStringLiteral("2 logical processors"), gb(15.5) + QStringLiteral(" RAM") }));

        QCOMPARE(dialog.memoryText()->text(), gb(15.5) + QStringLiteral(" of RAM"));
        QCOMPARE(dialog.memoryInUseText()->text(), gb(3.9) + QStringLiteral(" (25%)"));
        QCOMPARE(dialog.memoryBar()->value(), 25);
        QCOMPARE(dialog.swapText()->text(), gb(4.0));
        QCOMPARE(dialog.swapBar()->value(), 25);
        QCOMPARE(dialog.swapList()->topLevelItemCount(), 2);
        QCOMPARE(dialog.swapList()->topLevelItem(0)->text(1), QStringLiteral("Compressed memory"));
        QCOMPARE(dialog.swapList()->topLevelItem(1)->text(1), QStringLiteral("File"));

        // Reading again replaces what was shown; no swap says so.
        dialog.setReader([] {
            SystemInfo info;
            info.operatingSystem = QStringLiteral("Linux");
            info.processor.model = QStringLiteral("aarch64");
            info.processor.count = 1;
            info.memory.total = 512 * 1024;
            return info;
        });
        dialog.prepare();
        QCOMPARE(dialog.generalText().count(QStringLiteral("System:")), 1);
        QVERIFY(!dialog.generalText().contains(QStringLiteral("1 logical processor")));
        QVERIFY(dialog.generalText().contains(QStringLiteral("512 MB RAM")));
        QCOMPARE(dialog.swapText()->text(), QStringLiteral("Disabled"));
        QVERIFY(dialog.swapList()->isHidden());
        QVERIFY(dialog.swapBar()->isHidden());
        // No logo: nothing under the computer.
        QVERIFY(dialog.logo()->isHidden());
    }
};

QTEST_MAIN(TestSystemInfo)
#include "tst_systeminfo.moc"
