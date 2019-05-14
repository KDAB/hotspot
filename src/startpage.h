/*
  startpage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#pragma once

#include <QWidget>

namespace Ui {
class StartPage;
}

class QMenu;

class StartPage : public QWidget
{
    Q_OBJECT
public:
    explicit StartPage(QWidget* parent = nullptr);
    ~StartPage();

    void showStartPage();
    void showParseFileProgress();

    void setPathSettingsMenu(QMenu* menu);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

public slots:
    void onOpenFileError(const QString& errorMessage);
    void onParseFileProgress(float percent);

signals:
    void openFileButtonClicked();
    void recordButtonClicked();
    void stopParseButtonClicked();

private:
    void updateBackground();

    QScopedPointer<Ui::StartPage> ui;

    QPixmap m_background;
};
