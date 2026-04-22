#include "VirtualPenPlanner.h"

#include <QString>

namespace {
constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
constexpr quint64 FAT32_MAX = 32ULL * GiB;
constexpr quint64 MIN_EXFAT = 256ULL * 1024ULL * 1024ULL;
}

PartitionPlan VirtualPenPlanner::buildManyFat32ThenExfat(quint64 totalBytes, int requestedFat32Parts) {
    PartitionPlan plan;
    if (totalBytes < 1ULL * GiB) {
        plan.summary = "Nośnik jest za mały na sensowny podział.";
        return plan;
    }

    int maxFat32 = static_cast<int>(totalBytes / FAT32_MAX);
    if (maxFat32 < 1) {
        maxFat32 = 1;
    }

    int fat32Count = requestedFat32Parts;
    if (fat32Count <= 0) {
        // Tryb auto: tyle FAT32 ile sie miesci, ale zostaw reszte na exFAT
        fat32Count = maxFat32;
        // Jesli caly dysk pokryloby same FAT32, uzyj o jedna mniej (reszta -> exFAT)
        const quint64 coveredByFat32 = static_cast<quint64>(fat32Count) * FAT32_MAX;
        if (coveredByFat32 >= totalBytes && fat32Count > 1) {
            --fat32Count;
        }
    }
    if (fat32Count > maxFat32) {
        fat32Count = maxFat32;
    }

    quint64 used = 0;
    for (int i = 0; i < fat32Count; ++i) {
        const quint64 remaining = totalBytes - used;
        const bool isLast = (i == fat32Count - 1);

        quint64 partSize = (remaining > FAT32_MAX) ? FAT32_MAX : remaining;

        if (isLast) {
            const quint64 leftAfter = (remaining > partSize) ? (remaining - partSize) : 0;
            if (leftAfter > 0 && leftAfter < MIN_EXFAT) {
                // Reszta za mala na exFAT - wchlon do ostatniej FAT32 o ile nie przekroczy 32GiB
                if (partSize + leftAfter <= FAT32_MAX) {
                    partSize += leftAfter;
                }
                // Jesli przekroczyloby 32GiB - zostaw reszte (walidator to zlapie)
            }
        }

        PartitionSpec p;
        p.fs = "FAT32";
        p.label = QString("USB_FAT32_%1").arg(i + 1);
        p.sizeBytes = partSize;
        plan.partitions.push_back(p);
        used += partSize;

        if (used >= totalBytes) {
            break;
        }
    }

    if (totalBytes > used) {
        PartitionSpec ex;
        ex.fs = "exFAT";
        ex.label = "PLIKI";
        ex.sizeBytes = totalBytes - used;
        plan.partitions.push_back(ex);
    }

    int actualFat32Count = 0;
    for (const auto& part : plan.partitions) {
        if (part.fs == "FAT32") {
            ++actualFat32Count;
        }
    }

    int fat32Index = 1;
    for (auto& part : plan.partitions) {
        if (part.fs == "FAT32") {
            if (actualFat32Count == 1) {
                part.label = "SKANY";
            } else {
                part.label = QString("SKANY_%1").arg(fat32Index);
            }
            ++fat32Index;
        }
    }

    plan.summary = QString("FAT32 partycji: %1, exFAT: %2")
        .arg(actualFat32Count)
        .arg(plan.partitions.empty() ? 0 : (plan.partitions.back().fs == "exFAT" ? 1 : 0));

    return plan;
}
