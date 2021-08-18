/*
  perfoutputwidget.h

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

#ifndef PERFOUTPUTWIDGET_H
#define PERFOUTPUTWIDGET_H

#include <QWidget>

class PerfOutputWidget : public QWidget
{
    Q_OBJECT
public:
    PerfOutputWidget(QWidget* parent = nullptr);
    virtual ~PerfOutputWidget();

    virtual void addOutput(const QString&) = 0;
    virtual void clear() = 0;
    virtual void enableInput(bool enable) = 0;
    virtual void setInputVisible(bool visible) = 0;

signals:
    void sendInput(const QByteArray& input);
};

#endif // PERFOUTPUTWIDGET_H
