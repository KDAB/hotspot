/*
  startpage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "startpage.h"
#include "ui_startpage.h"

#include <QDebug>
#include <QMainWindow>
#include <QPainter>

StartPage::StartPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StartPage)
{
    ui->setupUi(this);

    connect(ui->openFileButton, &QAbstractButton::clicked, this, &StartPage::openFileButtonClicked);
    connect(ui->recordDataButton, &QAbstractButton::clicked, this, &StartPage::recordButtonClicked);
    connect(ui->stopParseButton, &QAbstractButton::clicked, this, &StartPage::stopParseButtonClicked);

    ui->openFileButton->setFocus();

    updateBackground();
}

StartPage::~StartPage() = default;

void StartPage::showStartPage()
{
    ui->loadingResultsErrorLabel->hide();
    ui->loadStack->setCurrentWidget(ui->openFilePage);
}

void StartPage::showParseFileProgress()
{
    ui->loadingResultsErrorLabel->hide();
    ui->loadStack->setCurrentWidget(ui->parseProgressPage);

    // Reset maximum to show throbber, we may not get progress notifications
    ui->openFileProgressBar->setMaximum(0);
}

void StartPage::setPathSettingsMenu(QMenu* menu)
{
    ui->pathSettings->setMenu(menu);
}

void StartPage::onOpenFileError(const QString& errorMessage)
{
    qWarning() << errorMessage;
    ui->loadingResultsErrorLabel->setText(errorMessage);
    ui->loadingResultsErrorLabel->show();
    ui->loadStack->setCurrentWidget(ui->openFilePage);
}

void StartPage::onParseFileProgress(float percent)
{
    const int scale = 1000;
    if (!ui->openFileProgressBar->maximum()) {
        ui->openFileProgressBar->setMaximum(scale);
    }
    ui->openFileProgressBar->setValue(static_cast<int>(percent * scale));
}

void StartPage::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const auto windowRect = rect();
    auto backgroundRect = QRectF(QPointF(0, 0), QSizeF(m_background.size()) / devicePixelRatioF());
    backgroundRect.moveBottomRight(windowRect.bottomRight());
    painter.drawPixmap(backgroundRect.toRect(), m_background);
}

void StartPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::PaletteChange) {
        updateBackground();
    }
}

void StartPage::updateBackground()
{
    const auto background = palette().window().color();
    const auto foreground = palette().windowText().color();

    if (qGray(background.rgb()) < qGray(foreground.rgb())) {
        // Dark color scheme
        m_background = QPixmap(QStringLiteral(":/images/background_dark.png"));
    } else {
        // Bright color scheme
        m_background = QPixmap(QStringLiteral(":/images/background_bright.png"));
    }
    m_background.setDevicePixelRatio(devicePixelRatioF());
}
