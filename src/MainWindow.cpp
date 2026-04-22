#include "MainWindow.h"

#include "CommandBuilder.h"
#include "VirtualPenPlanner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr quint64 GiB = 1024ULL * 1024ULL * 1024ULL;
constexpr quint64 FAT32_MAX = 32ULL * GiB;
constexpr quint64 MIN_EXFAT = 256ULL * 1024ULL * 1024ULL;
}

MainWindow::MainWindow() {
    setupUi();
    refreshDevices();
}

void MainWindow::setupUi() {
    setWindowTitle("USB FAT32 Creator v1.1.0");
    resize(980, 640);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel("Urządzenie fizyczne:"));

    m_deviceCombo = new QComboBox();
    top->addWidget(m_deviceCombo, 1);

    top->addWidget(new QLabel("Liczba FAT32 (0=auto):"));
    m_virtualCount = new QSpinBox();
    m_virtualCount->setRange(0, 64);
    m_virtualCount->setValue(1);
    top->addWidget(m_virtualCount);

    m_dryRun = new QCheckBox("Tryb bezpieczny (dry-run)");
    m_dryRun->setChecked(true);
    top->addWidget(m_dryRun);

    m_refreshBtn = new QPushButton("Odśwież");
    m_planBtn = new QPushButton("Zbuduj plan");
    m_testBtn = new QPushButton("Testuj");
    m_execBtn = new QPushButton("Wykonaj");

    top->addWidget(m_refreshBtn);
    top->addWidget(m_planBtn);
    top->addWidget(m_testBtn);
    top->addWidget(m_execBtn);

    root->addLayout(top);

    m_output = new QPlainTextEdit();
    m_output->setReadOnly(true);
    root->addWidget(m_output, 1);

    setCentralWidget(central);

    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_planBtn, &QPushButton::clicked, this, &MainWindow::buildPlan);
    connect(m_testBtn, &QPushButton::clicked, this, &MainWindow::testPlan);
    connect(m_execBtn, &QPushButton::clicked, this, &MainWindow::executePlan);
}

void MainWindow::refreshDevices() {
    m_disks = m_diskManager.listPhysicalDevices();
    m_deviceCombo->clear();

    for (const auto& d : m_disks) {
        const QString row = QString("%1 | %2 | %3 | %4")
            .arg(d.devicePath)
            .arg(d.model.isEmpty() ? "(unknown model)" : d.model)
            .arg(CommandBuilder::prettySize(d.sizeBytes))
            .arg(d.likelyUsb ? "USB/removable" : "internal?");
        m_deviceCombo->addItem(row);
    }

    m_output->appendPlainText("[" + QDateTime::currentDateTime().toString() + "] znaleziono urządzeń: "
        + QString::number(m_disks.size()));
}

bool MainWindow::hasSelectedDisk(PhysicalDisk& outDisk) const {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) {
        return false;
    }
    outDisk = m_disks[static_cast<size_t>(idx)];
    return true;
}

PartitionPlan MainWindow::buildCurrentPartitionPlan() const {
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(m_disks.size())) {
        return {};
    }
    const auto& disk = m_disks[static_cast<size_t>(idx)];
    return VirtualPenPlanner::buildManyFat32ThenExfat(disk.sizeBytes, m_virtualCount->value());
}

QStringList MainWindow::validatePlan(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    QStringList errors;

    if (plan.partitions.empty()) {
        errors << "Plan nie zawiera żadnych partycji.";
        return errors;
    }

    quint64 totalPlanned = 0;
    int exfatCount = 0;
    bool exfatIsLast = true;

    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        const int partNo = static_cast<int>(i + 1);

        if (part.sizeBytes == 0) {
            errors << QString("Partycja %1 ma rozmiar 0 B.").arg(partNo);
        }

        if (part.fs != "FAT32" && part.fs != "exFAT") {
            errors << QString("Partycja %1 ma nieobsługiwany system plików: %2.")
                .arg(partNo)
                .arg(part.fs);
        }

        if (part.fs == "FAT32" && part.sizeBytes > FAT32_MAX) {
            errors << QString("Partycja FAT32 #%1 przekracza 32 GiB.").arg(partNo);
        }

        if (part.fs == "exFAT") {
            ++exfatCount;
            if (i != plan.partitions.size() - 1) {
                exfatIsLast = false;
            }
            if (part.sizeBytes < MIN_EXFAT) {
                errors << QString("Partycja exFAT #%1 jest mniejsza niż 256 MiB.").arg(partNo);
            }
        }

        const quint64 newTotal = totalPlanned + part.sizeBytes;
        if (newTotal < totalPlanned) {
            errors << "Wykryto przepełnienie podczas sumowania rozmiarów partycji.";
            break;
        }
        totalPlanned = newTotal;
    }

    if (exfatCount > 1) {
        errors << "Plan zawiera więcej niż jedną partycję exFAT.";
    }
    if (exfatCount == 1 && !exfatIsLast) {
        errors << "Partycja exFAT musi być ostatnia w planie.";
    }

    if (totalPlanned != disk.sizeBytes) {
        errors << QString("Suma partycji (%1) nie zgadza się z rozmiarem nośnika (%2).")
            .arg(CommandBuilder::prettySize(totalPlanned))
            .arg(CommandBuilder::prettySize(disk.sizeBytes));
    }

    return errors;
}

void MainWindow::printPlanToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    m_output->appendPlainText("\n=== PLAN PARTYCJONOWANIA ===");
    m_output->appendPlainText("Urządzenie: " + disk.devicePath);
    m_output->appendPlainText("Podsumowanie: " + plan.summary);

    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        m_output->appendPlainText(QString("%1) %2 | %3 | %4")
            .arg(i + 1)
            .arg(part.fs)
            .arg(part.label)
            .arg(CommandBuilder::prettySize(part.sizeBytes)));
    }
}

void MainWindow::printSimulationToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const {
    m_output->appendPlainText("\n=== TEST SYMULACJI (bez zmian na dysku) ===");

    quint64 offset = 0;
    for (size_t i = 0; i < plan.partitions.size(); ++i) {
        const auto& part = plan.partitions[i];
        const quint64 start = offset;
        const quint64 end = offset + part.sizeBytes;

        m_output->appendPlainText(QString("P%1 | %2 | %3 | %4 | zakres: %5 -> %6")
            .arg(i + 1)
            .arg(part.fs)
            .arg(part.label)
            .arg(CommandBuilder::prettySize(part.sizeBytes))
            .arg(CommandBuilder::prettySize(start))
            .arg(CommandBuilder::prettySize(end)));

        offset = end;
    }

    m_output->appendPlainText(QString("Suma partycji: %1 / Rozmiar nośnika: %2")
        .arg(CommandBuilder::prettySize(offset))
        .arg(CommandBuilder::prettySize(disk.sizeBytes)));
}

void MainWindow::buildPlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    const PartitionPlan p = buildCurrentPartitionPlan();
    const QStringList errors = validatePlan(disk, p);
    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] Walidacja planu nie powiodła się:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::critical(this, "Błędny plan", "Walidacja planu nie powiodła się. Sprawdź log.");
        m_lastPartitionPlan = {};
        m_lastCommandPlan = {};
        return;
    }

    m_lastPartitionPlan = p;
    m_lastCommandPlan = CommandBuilder::build(disk, p);

    printPlanToOutput(disk, p);

    m_output->appendPlainText("\n=== PLAN POLECEŃ ===");
    for (const auto& c : m_lastCommandPlan.commands) {
        m_output->appendPlainText(c);
    }
}

void MainWindow::testPlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    const PartitionPlan p = buildCurrentPartitionPlan();
    const QStringList errors = validatePlan(disk, p);

    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] TEST: plan nie przeszedł walidacji:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::warning(this, "Test nieudany", "Plan nie przeszedł walidacji. Sprawdź log.");
        return;
    }

    m_output->appendPlainText("\n[OK] TEST: plan przeszedł walidację.");
    printPlanToOutput(disk, p);
    printSimulationToOutput(disk, p);
}

void MainWindow::executePlan() {
    PhysicalDisk disk;
    if (!hasSelectedDisk(disk)) {
        QMessageBox::warning(this, "Brak urządzenia", "Wybierz urządzenie fizyczne.");
        return;
    }

    if (m_lastPartitionPlan.partitions.empty() || m_lastCommandPlan.commands.empty()) {
        QMessageBox::information(this, "Brak planu", "Najpierw kliknij 'Zbuduj plan'.");
        return;
    }

    const QStringList errors = validatePlan(disk, m_lastPartitionPlan);
    if (!errors.isEmpty()) {
        m_output->appendPlainText("\n[ERR] Walidacja przed wykonaniem nie powiodła się:");
        for (const QString& err : errors) {
            m_output->appendPlainText(" - " + err);
        }
        QMessageBox::critical(this, "Błędny plan", "Plan jest niepoprawny. Zbuduj plan ponownie.");
        return;
    }

    if (m_dryRun->isChecked()) {
        m_output->appendPlainText("\n[DRY-RUN] Nie wykonano żadnych poleceń.");
        return;
    }

    const auto answer = QMessageBox::warning(
        this,
        "Potwierdzenie",
        QString("Uwaga: zostanie sformatowane urządzenie %1.\n"
            "Wszystkie dane na tym nośniku zostaną bezpowrotnie usunięte.\n"
            "Ta operacja jest nieodwracalna.\n\n"
            "Czy na pewno kontynuować?")
            .arg(disk.devicePath),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

#if defined(Q_OS_WIN)
    {
        const QString scriptPath = QDir::tempPath() + "/diskpart_script.txt";
        QFile scriptFile(scriptPath);
        if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_output->appendPlainText("[ERR] Nie można utworzyć pliku tymczasowego: " + scriptPath);
            return;
        }
        QTextStream stream(&scriptFile);
        for (const QString& cmd : m_lastCommandPlan.commands) {
            stream << cmd << "\n";
        }
        scriptFile.close();

        m_output->appendPlainText("[INFO] Skrypt diskpart zapisany do: " + scriptPath);
        m_output->appendPlainText("[RUN] diskpart /s " + scriptPath);

        QProcess proc;
        proc.start("diskpart", {"/s", scriptPath});
        if (!proc.waitForFinished(300000)) {
            m_output->appendPlainText("[ERR] timeout - diskpart nie zakończył się w ciągu 5 minut.");
            proc.kill();
            QFile::remove(scriptPath);
            return;
        }
        const QString stdOut = QString::fromUtf8(proc.readAllStandardOutput());
        const QString stdErr = QString::fromUtf8(proc.readAllStandardError());
        if (!stdOut.isEmpty()) {
            m_output->appendPlainText(stdOut);
        }
        if (!stdErr.isEmpty()) {
            m_output->appendPlainText("[ERR] " + stdErr);
        }
        QFile::remove(scriptPath);
    }
#else
    for (const QString& commandLine : m_lastCommandPlan.commands) {
        m_output->appendPlainText("[RUN] " + commandLine);
        QProcess p;
        p.start("/bin/sh", {"-c", commandLine});
        if (!p.waitForFinished(-1)) {
            m_output->appendPlainText("[ERR] timeout");
            continue;
        }
        const QString stdOut = QString::fromUtf8(p.readAllStandardOutput());
        const QString stdErr = QString::fromUtf8(p.readAllStandardError());
        if (!stdOut.isEmpty()) {
            m_output->appendPlainText(stdOut);
        }
        if (!stdErr.isEmpty()) {
            m_output->appendPlainText(stdErr);
        }
    }
#endif
}
