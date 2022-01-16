/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "costheaderview.h"

#include <QDebug>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QScopedValueRollback>

#include "costcontextmenu.h"

CostHeaderView::CostHeaderView(CostContextMenu* contextMenu, QWidget* parent)
    : QHeaderView(Qt::Horizontal, parent)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    setSectionsMovable(true);
    setFirstSectionMovable(false);
#endif
    setDefaultSectionSize(150);
    setStretchLastSection(false);
    connect(this, &QHeaderView::sectionCountChanged, this, [this]() { resizeColumns(false); });
    connect(this, &QHeaderView::sectionResized, this, [this](int index, int oldSize, int newSize) {
        if (m_isResizing)
            return;
        QScopedValueRollback<bool> guard(m_isResizing, true);
        if (index != 0) {
            // give/take space from first column
            resizeSection(0, sectionSize(0) - (newSize - oldSize));
        } else {
            // distribute space across all columns
            // use actual width as oldSize/newSize isn't reliable here
            const auto numSections = count();
            int usedWidth = 0;
            for (int i = 0; i < numSections; ++i)
                usedWidth += sectionSize(i);
            const auto diff = usedWidth - width();
            const auto numVisibleSections = numSections - hiddenSectionCount();
            if (numVisibleSections == 0)
                return;

            const auto diffPerSection = diff / numVisibleSections;
            const auto extraDiff = diff % numVisibleSections;
            for (int i = 1; i < numSections; ++i) {
                if (isSectionHidden(i)) {
                    continue;
                }
                auto newSize = sectionSize(i) - diffPerSection;
                if (i == numSections - 1)
                    newSize -= extraDiff;
                resizeSection(i, newSize);
            }
        }
    });

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QHeaderView::customContextMenuRequested, this, [this, contextMenu](const QPoint& pos) {
        const auto numSections = count();

        QMenu menu;
        auto resetSizes = menu.addAction(tr("Reset Column Sizes"));
        connect(resetSizes, &QAction::triggered, this, [this]() { resizeColumns(true); });

        if (numSections > 1) {
            auto* subMenu = menu.addMenu(tr("Visible Columns"));
            contextMenu->addToMenu(this, subMenu);
        }

        menu.exec(mapToGlobal(pos));
    });
}

CostHeaderView::~CostHeaderView() = default;

void CostHeaderView::resizeEvent(QResizeEvent* event)
{
    QHeaderView::resizeEvent(event);
    resizeColumns(false);
}

void CostHeaderView::resizeColumns(bool reset)
{
    QScopedValueRollback<bool> guard(m_isResizing, true);
    auto availableWidth = width();
    const auto defaultSize = defaultSectionSize();
    for (int i = count() - 1; i >= 0; --i) {
        if (i == 0) {
            resizeSection(0, availableWidth);
        } else {
            if (reset) {
                resizeSection(i, defaultSize);
            }
            if (!isSectionHidden(i)) {
                availableWidth -= sectionSize(i);
            }
        }
    }
}
