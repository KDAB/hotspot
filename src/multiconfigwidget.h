/*
  multiconfigwidget.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

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

#ifndef MULTICONFIGWIDGET_H
#define MULTICONFIGWIDGET_H

#include <KConfigGroup>
#include <QWidget>

class QComboBox;
class QPushButton;

class MultiConfigWidget : public QWidget
{
    Q_OBJECT
public:
    MultiConfigWidget(QWidget* parent = nullptr);
    ~MultiConfigWidget();

    QString currentConfig() const;

signals:
    void saveConfig(KConfigGroup group);
    void restoreConfig(const KConfigGroup& group);

public slots:
    void setConfig(const KConfigGroup& group);
    void saveConfigAs(const QString& name);
    void updateCurrentConfig();
    void selectConfig(const QString& name);
    void restoreCurrent();

private:
    KConfigGroup m_config;
    QComboBox* m_comboBox = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_removeButton = nullptr;
};

#endif // MULTICONFIGWIDGET_H
