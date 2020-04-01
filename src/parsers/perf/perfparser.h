/*
  perfparser.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

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

#include <atomic>
#include <memory>
#include <QObject>

#include <models/data.h>

// TODO: create a parser interface
class PerfParser : public QObject
{
    Q_OBJECT
public:
    explicit PerfParser(QObject* parent = nullptr);
    ~PerfParser();

    void startParseFile(const QString& path, const QString& sysroot, const QString& kallsyms, const QString& debugPaths,
                        const QString& extraLibPaths, const QString& appPath, const QString& arch);

    void filterResults(const Data::FilterAction& filter);

    void stop();

signals:
    void parsingStarted();
    void summaryDataAvailable(const Data::Summary& data);
    void bottomUpDataAvailable(const Data::BottomUpResults& data);
    void topDownDataAvailable(const Data::TopDownResults& data);
    void callerCalleeDataAvailable(const Data::CallerCalleeResults& data);
    void eventsAvailable(const Data::EventResults& events);
    void parsingFinished();
    void parsingFailed(const QString& errorMessage);
    void progress(float progress);
    void stopRequested();

private:
    // only set once after the initial startParseFile finished
    Data::BottomUpResults m_bottomUpResults;
    Data::CallerCalleeResults m_callerCalleeResults;
    Data::EventResults m_events;
    std::atomic<bool> m_isParsing;
    std::atomic<bool> m_stopRequested;
};
