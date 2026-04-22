#include <QtTest/QtTest>

#include "VirtualPenPlanner.h"

class TestVirtualPenPlanner : public QObject {
    Q_OBJECT

    static constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
    static constexpr quint64 MiB = 1024ULL * 1024ULL;
    static constexpr quint64 FAT32_MAX = 32ULL * GiB;
    static constexpr quint64 MIN_EXFAT = 256ULL * MiB;

private slots:
    // ----- size guard -----
    void tooSmall_noPartitions();
    void tooSmall_returnsMessage();

    // ----- partition count and clamping -----
    void requestedExceedsMax_isClamped();
    void requestedZero_usesAutoMax();
    void requestedNegative_treatedAsAuto();

    // ----- leftover absorption -----
    void leftoverBelowMinExfat_absorbedIntoLastFat32();
    void leftoverExactlyMinExfat_createsExfatPartition();
    void leftoverAboveMinExfat_createsExfatPartition();
    void leftoverZero_noExfatPartition();

    // ----- labels -----
    void singleFat32_labelIsSkany();
    void multipleFat32_labelsAreNumbered();
    void exfatPartition_labelIsPliki();

    // ----- filesystem types -----
    void fat32Partitions_haveCorrectFs();
    void exfatPartition_hasCorrectFs();

    // ----- sizes -----
    void fat32Partitions_haveCorrectSize();
    void singleFat32WithAbsorption_hasCorrectSize();

    // ----- summary -----
    void summary_noExfat_reflectsZeroExfat();
    void summary_withExfat_reflectsOneExfat();
    void summary_fat32Count_isCorrect();
};

// ----- size guard -----

void TestVirtualPenPlanner::tooSmall_noPartitions() {
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(512 * MiB, 1);
    QVERIFY(plan.partitions.empty());
}

void TestVirtualPenPlanner::tooSmall_returnsMessage() {
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(512 * MiB, 1);
    QVERIFY(!plan.summary.isEmpty());
}

// ----- partition count and clamping -----

void TestVirtualPenPlanner::requestedExceedsMax_isClamped() {
    // 64 GiB → max 2 FAT32; requesting 10 should be clamped to 2
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 10);
    int fat32Count = 0;
    for (const auto& p : plan.partitions) {
        if (p.fs == "FAT32") ++fat32Count;
    }
    QCOMPARE(fat32Count, 2);
}

void TestVirtualPenPlanner::requestedZero_usesAutoMax() {
    const quint64 total = 64ULL * GiB;
    auto planAuto = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    auto planExplicit = VirtualPenPlanner::buildManyFat32ThenExfat(total, 2);
    QCOMPARE(planAuto.partitions.size(), planExplicit.partitions.size());
}

void TestVirtualPenPlanner::requestedNegative_treatedAsAuto() {
    const quint64 total = 64ULL * GiB;
    auto planAuto = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    auto planNeg  = VirtualPenPlanner::buildManyFat32ThenExfat(total, -1);
    QCOMPARE(planAuto.partitions.size(), planNeg.partitions.size());
}

// ----- leftover absorption -----

void TestVirtualPenPlanner::leftoverBelowMinExfat_absorbedIntoLastFat32() {
    // 32 GiB + 100 MiB: leftover 100 MiB < MIN_EXFAT → absorbed, no exFAT
    const quint64 total = FAT32_MAX + 100ULL * MiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE((int)plan.partitions.size(), 1);
    QCOMPARE(plan.partitions[0].fs, QString("FAT32"));
}

void TestVirtualPenPlanner::leftoverExactlyMinExfat_createsExfatPartition() {
    // 32 GiB + exactly 256 MiB → leftover == MIN_EXFAT → NOT absorbed (condition is strict <)
    // → separate exFAT partition
    const quint64 total = FAT32_MAX + MIN_EXFAT;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE((int)plan.partitions.size(), 2);
    QCOMPARE(plan.partitions.back().fs, QString("exFAT"));
}

void TestVirtualPenPlanner::leftoverAboveMinExfat_createsExfatPartition() {
    // 32 GiB + 1 GiB: leftover 1 GiB > MIN_EXFAT → exFAT partition
    const quint64 total = FAT32_MAX + GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE((int)plan.partitions.size(), 2);
    QCOMPARE(plan.partitions.back().fs, QString("exFAT"));
    QCOMPARE(plan.partitions.back().sizeBytes, GiB);
}

void TestVirtualPenPlanner::leftoverZero_noExfatPartition() {
    // exactly 64 GiB, 2 FAT32 fills it entirely → no exFAT
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    for (const auto& p : plan.partitions) {
        QVERIFY(p.fs != "exFAT");
    }
}

// ----- labels -----

void TestVirtualPenPlanner::singleFat32_labelIsSkany() {
    // Only 1 FAT32 partition → label should be "SKANY" (not "SKANY_1")
    const quint64 total = FAT32_MAX + GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE((int)plan.partitions.size(), 2);
    QCOMPARE(plan.partitions[0].label, QString("SKANY"));
}

void TestVirtualPenPlanner::multipleFat32_labelsAreNumbered() {
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    QCOMPARE((int)plan.partitions.size(), 2);
    QCOMPARE(plan.partitions[0].label, QString("SKANY_1"));
    QCOMPARE(plan.partitions[1].label, QString("SKANY_2"));
}

void TestVirtualPenPlanner::exfatPartition_labelIsPliki() {
    const quint64 total = FAT32_MAX + GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE(plan.partitions.back().label, QString("PLIKI"));
}

// ----- filesystem types -----

void TestVirtualPenPlanner::fat32Partitions_haveCorrectFs() {
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    for (const auto& p : plan.partitions) {
        QCOMPARE(p.fs, QString("FAT32"));
    }
}

void TestVirtualPenPlanner::exfatPartition_hasCorrectFs() {
    const quint64 total = FAT32_MAX + GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE(plan.partitions.back().fs, QString("exFAT"));
}

// ----- sizes -----

void TestVirtualPenPlanner::fat32Partitions_haveCorrectSize() {
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    for (const auto& p : plan.partitions) {
        QCOMPARE(p.sizeBytes, FAT32_MAX);
    }
}

void TestVirtualPenPlanner::singleFat32WithAbsorption_hasCorrectSize() {
    const quint64 extra = 100ULL * MiB;
    const quint64 total = FAT32_MAX + extra;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QCOMPARE((int)plan.partitions.size(), 1);
    QCOMPARE(plan.partitions[0].sizeBytes, total);
}

// ----- summary -----

void TestVirtualPenPlanner::summary_noExfat_reflectsZeroExfat() {
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    QVERIFY(plan.summary.contains("exFAT: 0"));
}

void TestVirtualPenPlanner::summary_withExfat_reflectsOneExfat() {
    const quint64 total = FAT32_MAX + GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 1);
    QVERIFY(plan.summary.contains("exFAT: 1"));
}

void TestVirtualPenPlanner::summary_fat32Count_isCorrect() {
    const quint64 total = 64ULL * GiB;
    auto plan = VirtualPenPlanner::buildManyFat32ThenExfat(total, 0);
    QVERIFY(plan.summary.contains("FAT32 partycji: 2"));
}

QTEST_GUILESS_MAIN(TestVirtualPenPlanner)
#include "tst_VirtualPenPlanner.moc"
