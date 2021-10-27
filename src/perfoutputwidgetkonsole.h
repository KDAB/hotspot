/*
  perfoutputwidgetkonsole.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

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

#ifndef PERFOUTPUTWIDGETKONSOLE_H
#define PERFOUTPUTWIDGETKONSOLE_H

#include "perfoutputwidget.h"

#include <QWidget>

namespace KParts {
class ReadOnlyPart;
}

class QTemporaryFile;

class PerfOutputWidgetKonsole : public PerfOutputWidget
{
    Q_OBJECT
public:
    PerfOutputWidgetKonsole(KParts::ReadOnlyPart* part, QWidget* parent = nullptr);
    ~PerfOutputWidgetKonsole();

    static PerfOutputWidgetKonsole* create(QWidget* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;
    void addOutput(const QString& output) override;
    void clear() override;
    void enableInput(bool enable) override;
    void setInputVisible(bool visible) override;

private:
    void addPartToLayout();

    KParts::ReadOnlyPart* m_konsolePart = nullptr;
    QTemporaryFile* m_konsoleFile = nullptr;
    bool m_inputEnabled = false;
    static QString m_tailPath;
    QByteArray m_inputBuffer;
};

#endif // PERFOUTPUTWIDGETKONSOLE_H
