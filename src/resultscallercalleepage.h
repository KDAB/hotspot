/*
  resultscallercalleepage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QWidget>

namespace Ui {
class ResultsCallerCalleePage;
}

namespace Data {
struct Symbol;
}

class QSortFilterProxyModel;
class QModelIndex;

class PerfParser;
class CallerCalleeModel;

class ResultsCallerCalleePage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsCallerCalleePage(PerfParser *parser, QWidget *parent = nullptr);
    ~ResultsCallerCalleePage();

    void setSysroot(const QString& path);
    void setAppPath(const QString& path);

    void jumpToCallerCallee(const Data::Symbol &symbol);

private slots:
    void onSourceMapContextMenu(const QPoint& pos);
    void onSourceMapActivated(const QModelIndex& index);

signals:
    void navigateToCode(const QString &url, int lineNumber, int columnNumber);

private:
    struct SourceMapLocation
    {
        inline explicit operator bool() const
        {
            return !path.isEmpty();
        }

        QString path;
        int lineNumber = -1;
    };
    SourceMapLocation toSourceMapLocation(const QModelIndex& index);

    QScopedPointer<Ui::ResultsCallerCalleePage> ui;

    CallerCalleeModel* m_callerCalleeCostModel;
    QSortFilterProxyModel* m_callerCalleeProxy;

    QString m_sysroot;
    QString m_appPath;
};
