/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHeaderView>

class CostContextMenu;

class CostHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit CostHeaderView(CostContextMenu* contextMenu, QWidget* parent = nullptr);
    ~CostHeaderView();

    void setAutoResize(bool autoResize)
    {
        m_autoResize = autoResize;
    }

private:
    void resizeEvent(QResizeEvent* event) override;
    void resizeColumns(bool reset);

    bool m_isResizing = false;
    bool m_autoResize = true;
};
