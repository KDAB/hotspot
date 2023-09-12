/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QWidget>

#include "processlist.h"
#include "recordhost.h"

#include <memory>

class QTimer;
class QTemporaryFile;

namespace Ui {
class RecordPage;
}

class PerfRecord;
class ProcessModel;
class ProcessFilterModel;
class PerfOutputWidget;

namespace KParts {
class ReadOnlyPart;
}

class RecordPage : public QWidget
{
    Q_OBJECT
public:
    explicit RecordPage(QWidget* parent = nullptr);
    ~RecordPage();

    void showRecordPage();
    void stopRecording();

signals:
    void homeButtonClicked();
    void openFile(QString filePath);

private slots:
    void onApplicationNameChanged(const QString& filePath);
    void onStartRecordingButtonClicked(bool checked);
    void onOutputFileNameChanged(const QString& filePath);
    void onOutputFileUrlChanged(const QUrl& fileUrl);
    void onOutputFileNameSelected(const QString& filePath);

    void updateProcesses();
    void updateProcessesFinished();

private:
    void recordingStopped();
    void updateRecordType();
    void appendOutput(const QString& text);
    void setError(const QString& message);

    std::unique_ptr<Ui::RecordPage> ui;

    RecordHost* m_recordHost;
    PerfRecord* m_perfRecord;
    QString m_resultsFile;
    QElapsedTimer m_recordTimer;
    QTimer* m_updateRuntimeTimer;
    KParts::ReadOnlyPart* m_konsolePart = nullptr;
    QTemporaryFile* m_konsoleFile = nullptr;
    PerfOutputWidget* m_perfOutput;

    ProcessModel* m_processModel;
    ProcessFilterModel* m_processProxyModel;

    QFutureWatcher<ProcDataList>* m_watcher;
};
