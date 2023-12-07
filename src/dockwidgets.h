/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kddockwidgets/kddockwidgets_version.h>

#if KDDOCKWIDGETS_VERSION < KDDOCKWIDGETS_VERSION_CHECK(2, 0, 0)
namespace KDDockWidgets {
class DockWidgetBase;
class DockWidget;
class MainWindow;
}

using DockWidget = KDDockWidgets::DockWidget;
using DockMainWindow = KDDockWidgets::MainWindow;
using CoreDockWidget = KDDockWidgets::DockWidgetBase;
#else
namespace KDDockWidgets {
namespace QtWidgets {
class DockWidget;
class MainWindow;
}

namespace Core {
class DockWidget;
}
}

using DockWidget = KDDockWidgets::QtWidgets::DockWidget;
using CoreDockWidget = KDDockWidgets::Core::DockWidget;
using DockMainWindow = KDDockWidgets::QtWidgets::MainWindow;
#endif // KDDOCKWIDGETS_VERSION < KDDOCKWIDGETS_VERSION_CHECK(2, 0, 0)
