#pragma once

#include "DiskManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void refreshDevices();
    void buildPlan();
    void testPlan();
    void executePlan();

private:
    void setupUi();
    bool hasSelectedDisk(PhysicalDisk& outDisk) const;
    PartitionPlan buildCurrentPartitionPlan() const;
    QStringList validatePlan(const PhysicalDisk& disk, const PartitionPlan& plan) const;
    void printPlanToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const;
    void printSimulationToOutput(const PhysicalDisk& disk, const PartitionPlan& plan) const;

    DiskManager m_diskManager;
    std::vector<PhysicalDisk> m_disks;

    QComboBox* m_deviceCombo = nullptr;
    QSpinBox* m_virtualCount = nullptr;
    QCheckBox* m_dryRun = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_planBtn = nullptr;
    QPushButton* m_testBtn = nullptr;
    QPushButton* m_execBtn = nullptr;

    PartitionPlan m_lastPartitionPlan;
    CommandPlan m_lastCommandPlan;
};
