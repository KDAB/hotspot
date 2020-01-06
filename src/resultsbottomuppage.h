/*
  resultsbottomuppage.h

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

#include <QWidget>

class QMenu;

namespace Ui {
class ResultsBottomUpPage;
}

namespace Data {
struct Symbol;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;

class ResultsBottomUpPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsBottomUpPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                 QMenu* exportMenu, QWidget* parent = nullptr);
    ~ResultsBottomUpPage();

    void clear();

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);

private:
    QScopedPointer<Ui::ResultsBottomUpPage> ui;
};
