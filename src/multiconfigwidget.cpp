/*
  multiconfigwidget.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "multiconfigwidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

MultiConfigWidget::MultiConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setMargin(0);

    m_comboBox = new QComboBox(this);
    m_comboBox->setEditable(true);
    m_comboBox->setInsertPolicy(QComboBox::InsertAtCurrent);
    m_comboBox->setDisabled(true);
    layout->addWidget(m_comboBox);

    connect(m_comboBox->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        m_config.deleteGroup(m_comboBox->currentData().toString());
        saveConfigAs(m_comboBox->currentText());
        m_comboBox->setItemData(m_comboBox->currentIndex(), m_comboBox->currentText());
        m_config.sync();
    });

    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { selectConfig(m_comboBox->currentData().toString()); });

    m_copyButton = new QPushButton(this);
    m_copyButton->setText(tr("Copy Config"));
    layout->addWidget(m_copyButton);

    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        const QString name = tr("Config %1").arg(m_comboBox->count() + 1);
        saveConfigAs(name);
        m_comboBox->addItem(name, name);
        m_comboBox->setDisabled(false);
    });

    m_removeButton = new QPushButton(this);
    m_removeButton->setText(tr("Remove Config"));
    layout->addWidget(m_removeButton);

    connect(m_removeButton, &QPushButton::clicked, this, [this] {
        m_config.deleteGroup(m_comboBox->currentData().toString());
        m_comboBox->removeItem(m_comboBox->currentIndex());

        if (m_comboBox->count() == 0) {
            m_comboBox->setDisabled(true);
        } else {
            selectConfig(m_comboBox->currentData().toString());
        }
    });

    setLayout(layout);
}

MultiConfigWidget::~MultiConfigWidget() = default;

QString MultiConfigWidget::currentConfig() const
{
    return m_comboBox->currentData().toString();
}

void MultiConfigWidget::setConfig(const KConfigGroup& group)
{
    m_comboBox->clear();
    m_config = group;

    const auto groups = m_config.groupList();
    for (const auto& config : groups) {
        if (m_config.hasGroup(config)) {
            // item data is used to get the old name after renaming
            m_comboBox->addItem(config, config);
            m_comboBox->setDisabled(false);
        }
    }
}

void MultiConfigWidget::saveConfigAs(const QString& name)
{
    if (!name.isEmpty()) {
        emit saveConfig(m_config.group(name));
    }
}

void MultiConfigWidget::updateCurrentConfig()
{
    if (m_comboBox->currentIndex() != -1) {
        saveConfigAs(m_comboBox->currentData().toString());
    }
}

void MultiConfigWidget::selectConfig(const QString& name)
{
    m_config.sync();
    if (!name.isEmpty() && m_config.hasGroup(name)) {
        emit restoreConfig(m_config.group(name));
    }
}

void MultiConfigWidget::restoreCurrent()
{
    if (m_comboBox->currentIndex() != -1) {
        selectConfig(m_comboBox->currentData().toString());
    }
}
