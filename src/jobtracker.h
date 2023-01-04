/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <ThreadWeaver/ThreadWeaver>
#include <QObject>
#include <QPointer>

class JobTracker
{
public:
    explicit JobTracker(QObject* context)
        : m_context(context)
    {
    }

    bool isJobRunning() const
    {
        return m_context && m_isRunning;
    }

    template<typename Job, typename SetData>
    void startJob(Job&& job, SetData&& setData)
    {
        using namespace ThreadWeaver;
        const auto jobId = ++m_currentJobId;
        auto jobCancelled = [context = m_context, jobId, currentJobId = &m_currentJobId]() {
            return !context || jobId != (*currentJobId);
        };
        auto maybeSetData = [jobCancelled, setData = std::forward<SetData>(setData),
                             isRunning = &m_isRunning](auto&& results) {
            if (!jobCancelled()) {
                setData(std::forward<decltype(results)>(results));
                *isRunning = false;
            }
        };

        m_isRunning = true;
        stream() << make_job([context = m_context, job = std::forward<Job>(job), maybeSetData = std::move(maybeSetData),
                              jobCancelled = std::move(jobCancelled)]() mutable {
            auto results = job(jobCancelled);
            if (jobCancelled())
                return;

            QMetaObject::invokeMethod(
                context.data(),
                [results = std::move(results), maybeSetData = std::move(maybeSetData)]() mutable {
                    maybeSetData(std::move(results));
                },
                Qt::QueuedConnection);
        });
    }

private:
    QPointer<QObject> m_context;
    std::atomic<uint> m_currentJobId;
    bool m_isRunning = false;
};
