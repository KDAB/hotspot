/*
  dockwidgetsetup.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "dockwidgetsetup.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/FrameworkWidgetFactory.h>

namespace {
class DockingArea : public KDDockWidgets::MainWindow
{
    Q_OBJECT
public:
    using MainWindow::MainWindow;

protected:
    QMargins centerWidgetMargins() const override
    {
        return {};
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (object == centralWidget() && event->type() == QEvent::Paint) {
            // don't paint the line in the central widget of KDDockWidgets
            // TODO: fix this somehow via proper API upstream
            return true;
        }
        return QObject::eventFilter(object, event);
    }
};
}

void setupDockWidgets()
{
    KDDockWidgets::Config::self().setFlags(KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible
                                           | KDDockWidgets::Config::Flag_TabsHaveCloseButton);
    KDDockWidgets::DefaultWidgetFactory::s_dropIndicatorType = KDDockWidgets::DropIndicatorType::Segmented;
}

KDDockWidgets::MainWindow* createDockingArea(const QString& id, QWidget* parent)
{
    auto ret = new DockingArea(id, KDDockWidgets::MainWindowOption_None, parent);

    // let it behave like a normal nested widget
    ret->setWindowFlag(Qt::Window, false);
    ret->centralWidget()->installEventFilter(ret);
    return ret;
}

#include "dockwidgetsetup.moc"
