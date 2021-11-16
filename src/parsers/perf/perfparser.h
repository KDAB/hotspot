/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <atomic>
#include <memory>
#include <QObject>

#include <models/data.h>

class QUrl;
class QTemporaryFile;

// TODO: create a parser interface
class PerfParser : public QObject
{
    Q_OBJECT
public:
    explicit PerfParser(QObject* parent = nullptr);
    ~PerfParser();

    void startParseFile(const QString& path);

    void filterResults(const Data::FilterAction& filter);

    void stop();

    void exportResults(const QUrl& url);

    Data::BottomUpResults bottomUpResults() const
    {
        return m_bottomUpResults;
    }
    Data::CallerCalleeResults callerCalleeResults() const
    {
        return m_callerCalleeResults;
    }
    Data::EventResults eventResults() const
    {
        return m_events;
    }

signals:
    void parsingStarted();
    void summaryDataAvailable(const Data::Summary& data);
    void bottomUpDataAvailable(const Data::BottomUpResults& data);
    void topDownDataAvailable(const Data::TopDownResults& data);
    void perLibraryDataAvailable(const Data::PerLibraryResults& data);
    void callerCalleeDataAvailable(const Data::CallerCalleeResults& data);
    void tracepointDataAvailable(const Data::TracepointResults& data);
    void frequencyDataAvailable(const Data::FrequencyResults& data);
    void eventsAvailable(const Data::EventResults& events);
    void parsingFinished();
    void parsingFailed(const QString& errorMessage);
    void progress(float progress);
    void stopRequested();

    void parserWarning(const QString& errorMessage);
    void exportFinished(const QUrl& url);

private:
    friend class TestPerfParser;
    QString decompressIfNeeded(const QString& path);

    // only set once after the initial startParseFile finished
    QStringList m_parserArgs;
    Data::BottomUpResults m_bottomUpResults;
    Data::CallerCalleeResults m_callerCalleeResults;
    Data::TracepointResults m_tracepointResults;
    Data::EventResults m_events;
    Data::FrequencyResults m_frequencyResults;
    std::atomic<bool> m_isParsing;
    std::atomic<bool> m_stopRequested;
    std::unique_ptr<QTemporaryFile> m_decompressed;
};
