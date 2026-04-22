#include <QtTest/QtTest>

#include "CommandBuilder.h"

class TestCommandBuilder : public QObject {
    Q_OBJECT

    static constexpr quint64 KiB = 1024ULL;
    static constexpr quint64 MiB = 1024ULL * 1024ULL;
    static constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
    static constexpr quint64 TiB = 1024ULL * 1024ULL * 1024ULL * 1024ULL;

private slots:
    // prettySize
    void prettySize_zero_returnsBytes();
    void prettySize_subKilobyte_returnsBytes();
    void prettySize_exactKilobyte_returnsKB();
    void prettySize_exactMegabyte_returnsMB();
    void prettySize_exactGigabyte_returnsGB();
    void prettySize_exactTerabyte_returnsTB();
    void prettySize_fractionalKB_oneDecimalPlace();
    void prettySize_fractionalGB_oneDecimalPlace();
    void prettySize_bytes_noDecimalPlace();

    // build – platform-independent structural properties
    void build_titleContainsDevicePath();
    void build_returnsNonEmptyCommands();

    // build – Linux-specific
    void build_linux_firstCommandWipefs();
    void build_linux_secondCommandMklabel();
    void build_linux_fat32MkfsCommand();
    void build_linux_exfatMkfsCommand();
    void build_linux_partitionDeviceNames();
    void build_linux_partedRangesAreContiguous();
};

// ---------- prettySize ----------

void TestCommandBuilder::prettySize_zero_returnsBytes() {
    QCOMPARE(CommandBuilder::prettySize(0), QString("0 B"));
}

void TestCommandBuilder::prettySize_subKilobyte_returnsBytes() {
    QCOMPARE(CommandBuilder::prettySize(512), QString("512 B"));
}

void TestCommandBuilder::prettySize_exactKilobyte_returnsKB() {
    QCOMPARE(CommandBuilder::prettySize(KiB), QString("1.0 KB"));
}

void TestCommandBuilder::prettySize_exactMegabyte_returnsMB() {
    QCOMPARE(CommandBuilder::prettySize(MiB), QString("1.0 MB"));
}

void TestCommandBuilder::prettySize_exactGigabyte_returnsGB() {
    QCOMPARE(CommandBuilder::prettySize(GiB), QString("1.0 GB"));
}

void TestCommandBuilder::prettySize_exactTerabyte_returnsTB() {
    QCOMPARE(CommandBuilder::prettySize(TiB), QString("1.0 TB"));
}

void TestCommandBuilder::prettySize_fractionalKB_oneDecimalPlace() {
    // 1536 bytes = 1.5 KB
    QCOMPARE(CommandBuilder::prettySize(1536), QString("1.5 KB"));
}

void TestCommandBuilder::prettySize_fractionalGB_oneDecimalPlace() {
    // 1.5 GB
    QCOMPARE(CommandBuilder::prettySize(3ULL * GiB / 2), QString("1.5 GB"));
}

void TestCommandBuilder::prettySize_bytes_noDecimalPlace() {
    // Values under 1 KB have no decimal
    QCOMPARE(CommandBuilder::prettySize(1023), QString("1023 B"));
}

// ---------- build – structural (all platforms) ----------

static PhysicalDisk makeDisk(const QString& path = "/dev/sdb") {
    PhysicalDisk d;
    d.devicePath = path;
    d.displayName = path;
    d.sizeBytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
    d.removable = true;
    d.likelyUsb = true;
    return d;
}

static PartitionSpec makeSpec(const QString& fs, const QString& label, quint64 sizeBytes) {
    PartitionSpec p;
    p.fs = fs;
    p.label = label;
    p.sizeBytes = sizeBytes;
    return p;
}

void TestCommandBuilder::build_titleContainsDevicePath() {
    PhysicalDisk disk = makeDisk("/dev/sdc");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);
    QVERIFY(cp.title.contains("/dev/sdc"));
}

void TestCommandBuilder::build_returnsNonEmptyCommands() {
    PhysicalDisk disk = makeDisk();
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);
    QVERIFY(!cp.commands.empty());
}

// ---------- build – Linux-specific ----------

void TestCommandBuilder::build_linux_firstCommandWipefs() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);
    QVERIFY(cp.commands[0].startsWith("wipefs -a /dev/sdb"));
}

void TestCommandBuilder::build_linux_secondCommandMklabel() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);
    QVERIFY(cp.commands[1].contains("mklabel gpt"));
}

void TestCommandBuilder::build_linux_fat32MkfsCommand() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);

    bool foundMkfs = false;
    for (const auto& cmd : cp.commands) {
        if (cmd.startsWith("mkfs.vfat")) {
            foundMkfs = true;
            QVERIFY(cmd.contains("-F 32"));
            QVERIFY(cmd.contains("SKANY"));
            QVERIFY(cmd.contains("/dev/sdb1"));
        }
    }
    QVERIFY(foundMkfs);
}

void TestCommandBuilder::build_linux_exfatMkfsCommand() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    plan.partitions.push_back(makeSpec("exFAT", "PLIKI", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);

    bool foundExfat = false;
    for (const auto& cmd : cp.commands) {
        if (cmd.startsWith("mkfs.exfat")) {
            foundExfat = true;
            QVERIFY(cmd.contains("PLIKI"));
            QVERIFY(cmd.contains("/dev/sdb2"));
        }
    }
    QVERIFY(foundExfat);
}

void TestCommandBuilder::build_linux_partitionDeviceNames() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY_1", 32ULL * GiB));
    plan.partitions.push_back(makeSpec("FAT32", "SKANY_2", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);

    bool foundPart1 = false;
    bool foundPart2 = false;
    for (const auto& cmd : cp.commands) {
        if (cmd.contains("/dev/sdb1")) foundPart1 = true;
        if (cmd.contains("/dev/sdb2")) foundPart2 = true;
    }
    QVERIFY(foundPart1);
    QVERIFY(foundPart2);
}

void TestCommandBuilder::build_linux_partedRangesAreContiguous() {
#ifndef Q_OS_LINUX
    QSKIP("Linux-only test");
#endif
    // Build a 2-partition plan and verify the parted mkpart commands
    // use non-overlapping, adjacent MiB ranges starting at 1MiB.
    PhysicalDisk disk = makeDisk("/dev/sdb");
    PartitionPlan plan;
    plan.partitions.push_back(makeSpec("FAT32", "SKANY", 32ULL * GiB));
    plan.partitions.push_back(makeSpec("exFAT", "PLIKI", 32ULL * GiB));
    CommandPlan cp = CommandBuilder::build(disk, plan);

    // Collect parted mkpart commands
    QStringList mkparts;
    for (const auto& cmd : cp.commands) {
        if (cmd.contains("mkpart")) mkparts << cmd;
    }
    QCOMPARE(mkparts.size(), 2);

    // First partition: starts at 1MiB
    QVERIFY(mkparts[0].contains(" 1MiB "));
    // Compute expected end of first partition: 1 + 32*1024 = 32769
    QVERIFY(mkparts[0].contains("32769MiB"));
    // Second partition starts where first ends
    QVERIFY(mkparts[1].contains("32769MiB"));
}

QTEST_GUILESS_MAIN(TestCommandBuilder)
#include "tst_CommandBuilder.moc"
