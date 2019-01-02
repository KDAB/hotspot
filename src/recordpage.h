/*
  recordpage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QWidget>

#include "processlist.h"

class QTimer;

namespace Ui {
class RecordPage;
}

class PerfRecord;
class ProcessModel;
class ProcessFilterModel;

enum RecordType
{
    LaunchApplication = 0,
    AttachToProcess,
    ProfileSystem,
    NUM_RECORD_TYPES
};
Q_DECLARE_METATYPE(RecordType)

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
    void onWorkingDirectoryNameChanged(const QString& folderPath);
    void onViewPerfRecordResultsButtonClicked();
    void onOutputFileNameChanged(const QString& filePath);
    void onOutputFileUrlChanged(const QUrl& fileUrl);
    void onOutputFileNameSelected(const QString& filePath);
    void updateOffCpuCheckboxState();

    void updateProcesses();
    void updateProcessesFinished();

private:
    void recordingStopped();
    void updateRecordType();
    void appendOutput(const QString& text);
    void setError(const QString& message);

    QScopedPointer<Ui::RecordPage> ui;

    PerfRecord* m_perfRecord;
    QString m_resultsFile;
    QElapsedTimer m_recordTimer;
    QTimer* m_updateRuntimeTimer;

    ProcessModel* m_processModel;
    ProcessFilterModel* m_processProxyModel;

    QFutureWatcher<ProcDataList>* m_watcher;
};
