/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dockwidgetsetup.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/kddockwidgets_version.h>

#if KDDOCKWIDGETS_VERSION < KDDOCKWIDGETS_VERSION_CHECK(2, 0, 0)
#include <kddockwidgets/DefaultWidgetFactory>
#include <kddockwidgets/MainWindow.h>
#else
#include <kddockwidgets/ViewFactory.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>
#endif // KDDOCKWIDGETS_VERSION < KDDOCKWIDGETS_VERSION_CHECK(2, 0, 0)

namespace {
class DockingArea : public DockMainWindow
{
    Q_OBJECT
public:
    using DockMainWindow::MainWindow;

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
    constexpr auto flags =
        KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible | KDDockWidgets::Config::Flag_TabsHaveCloseButton;

#if KDDOCKWIDGETS_VERSION < KDDOCKWIDGETS_VERSION_CHECK(2, 0, 0)
    KDDockWidgets::Config::self().setFlags(flags);
    KDDockWidgets::DefaultWidgetFactory::s_dropIndicatorType = KDDockWidgets::DropIndicatorType::Segmented;

#else
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    KDDockWidgets::Config::self().setFlags(flags);
    KDDockWidgets::Core::ViewFactory::s_dropIndicatorType = KDDockWidgets::DropIndicatorType::Segmented;
#endif
}

DockMainWindow* createDockingArea(const QString& id, QWidget* parent)
{
    auto ret = new DockingArea(id, KDDockWidgets::MainWindowOption_None, parent);

    // let it behave like a normal nested widget
    ret->setWindowFlag(Qt::Window, false);
    ret->centralWidget()->installEventFilter(ret);
    return ret;
}

#include "dockwidgetsetup.moc"
