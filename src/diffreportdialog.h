/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

namespace Ui {
class DiffReportDialog;
}

class DiffReportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DiffReportDialog(QWidget* parent = nullptr);
    ~DiffReportDialog();

    QString fileA() const;
    QString fileB() const;

private:
    QScopedPointer<Ui::DiffReportDialog> ui;
};
