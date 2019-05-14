/**************************************************************************
**
** This code is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#pragma once

#include <QList>
#include <QString>

struct ProcData
{
    QString ppid;
    QString name;
    QString state;
    QString user;

    inline bool equals(const ProcData& other) const
    {
        return ppid == other.ppid && name == other.name && state == other.state && user == other.user;
    }
};
Q_DECLARE_TYPEINFO(ProcData, Q_MOVABLE_TYPE);

inline bool operator<(const ProcData& l, const ProcData& r)
{
    return l.ppid < r.ppid;
}

inline bool operator==(const ProcData& l, const ProcData& r)
{
    return l.ppid == r.ppid;
}

QDebug operator<<(QDebug d, const ProcData& data);

using ProcDataList = QVector<ProcData>;
ProcDataList processList();
