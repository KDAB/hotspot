/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dockwidgetsetup.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/FrameworkWidgetFactory.h>
#include <kddockwidgets/MainWindow.h>

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
