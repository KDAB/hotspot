/*
    SPDX-FileCopyrightText: Volker Krause <volker.krause@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <memory>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

    void setTitle(const QString& title);
    void setText(const QString& text);
    void setLogo(const QString& iconFileName);

private:
    std::unique_ptr<Ui::AboutDialog> ui;
};
