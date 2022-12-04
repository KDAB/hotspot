/*
    SPDX-FileCopyrightText: Volker Krause <volker.krause@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QWidget::close);
}

AboutDialog::~AboutDialog() { }

void AboutDialog::setTitle(const QString& title)
{
    ui->titleLabel->setText(title);
}

void AboutDialog::setText(const QString& text)
{
    ui->textLabel->setText(text);
}

void AboutDialog::setLogo(const QString& iconFileName)
{
    QPixmap pixmap(iconFileName);
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    const auto maxWidth = 100 * devicePixelRatioF();
    // scale down pixmap (while keeping aspect ratio) if required
    if (pixmap.width() > maxWidth) {
        pixmap = pixmap.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    ui->logoLabel->setPixmap(pixmap);
}
