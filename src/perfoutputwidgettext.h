/*
  perfoutputwidgettext.h

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

#ifndef PERFOUTPUTWIDGETTEXT_H
#define PERFOUTPUTWIDGETTEXT_H

#include "perfoutputwidget.h"
#include <QWidget>

class QTemporaryFile;
class QTextEdit;
class QLineEdit;

class PerfOutputWidgetText : public PerfOutputWidget
{
    Q_OBJECT
public:
    PerfOutputWidgetText(QWidget* parent = nullptr);
    ~PerfOutputWidgetText() override;

    void addOutput(const QString& output) override;
    void clear() override;
    void enableInput(bool enable) override;
    void setInputVisible(bool visible) override;

private:
    QTextEdit* m_perfOutputTextEdit = nullptr;
    QLineEdit* m_perfInputEdit = nullptr;
};

#endif // PERFOUTPUTWIDGETTEXT_H
