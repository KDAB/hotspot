/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022-2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "multiconfigwidget.h"

#include <algorithm>

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

#include <KUrlRequester>

#include "ui_multiconfigwidget.h"

MultiConfigWidget::MultiConfigWidget(QWidget* parent)
    : QWidget(parent)
    , m_configWidget(new Ui::MultiConfigWidget)
{
    m_configWidget->setupUi(this);

    connect(m_configWidget->currentConfigComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { loadConfig(currentConfig()); });

    connect(this, &MultiConfigWidget::configsChanged, this, [this]() {
        m_configWidget->currentConfigComboBox->setDisabled(m_configWidget->currentConfigComboBox->count() == 0);
    });

    m_configWidget->currentConfigComboBox->setEnabled(true);

    connect(m_configWidget->copyButton, &QPushButton::pressed, this, [this] {
        const auto name = m_configWidget->currentConfigComboBox->currentText();
        auto copyName = QStringLiteral("Copy of %1").arg(name);
        if (name.isEmpty()) {
            copyName = QStringLiteral("Config");
        }
        saveConfig(copyName);
        m_configWidget->currentConfigComboBox->addItem(copyName, copyName);
        m_configWidget->currentConfigComboBox->setCurrentIndex(m_configWidget->currentConfigComboBox->count() - 1);
        emit configsChanged();
    });

    connect(m_configWidget->deleteButton, &QPushButton::pressed, this, [this] {
        const auto currentConfig = m_configWidget->currentConfigComboBox->currentText();
        m_configWidget->currentConfigComboBox->removeItem(m_configWidget->currentConfigComboBox->currentIndex());
        m_group.deleteGroup(currentConfig);
        emit configsChanged();
    });

    connect(m_configWidget->currentConfigComboBox->lineEdit(), &QLineEdit::returnPressed, this, [this] {
        const auto oldName = m_configWidget->currentConfigComboBox->currentData().toString();
        if (!oldName.isEmpty()) {
            m_group.deleteGroup(oldName);
        }

        auto newName = m_configWidget->currentConfigComboBox->currentText();
        if (newName.isEmpty()) {
            newName = QStringLiteral("Config %1").arg(m_configWidget->currentConfigComboBox->currentIndex());
        }
        m_configWidget->currentConfigComboBox->setItemData(m_configWidget->currentConfigComboBox->currentIndex(),
                                                           newName);
        saveConfig(newName);
    });
}

MultiConfigWidget::~MultiConfigWidget() = default;

void MultiConfigWidget::setChildWidget(QWidget* widget, const QVector<QWidget*>& formWidgets)
{
    m_formWidgets = formWidgets;
    m_child = widget;
    m_child->setParent(this);
    auto placeholder = m_configWidget->layout->replaceWidget(m_configWidget->placeholder, m_child);
    Q_ASSERT(placeholder);
    delete placeholder;
}

void MultiConfigWidget::setConfigGroup(const KConfigGroup& group)
{
    m_group = group;
    if (!m_group.isValid()) {
        return;
    }

    m_configWidget->currentConfigComboBox->clear();
    auto configGroups = configs();
    for (const auto& config : configGroups) {
        m_configWidget->currentConfigComboBox->addItem(config, config);
    }

    m_configWidget->currentConfigComboBox->setCurrentIndex(0);

    if (!configGroups.isEmpty()) {
        loadConfig(currentConfig());
    }
    emit configsChanged();
}

void MultiConfigWidget::loadConfig(const QString& name)
{
    if (m_saving) {
        return;
    }

    if (m_child == nullptr) {
        return;
    }

    if (name.isEmpty() || !m_group.isValid()) {
        return;
    }

    if (!m_group.hasGroup(name)) {
        return;
    }

    const auto& group = m_group.group(name);

    for (auto formWidget : std::as_const(m_formWidgets)) {
        const auto& name = formWidget->objectName();
        if (auto widget = qobject_cast<QLineEdit*>(formWidget)) {
            auto text = group.readEntry(name, QString {});
            widget->setText(text);
        } else if (auto widget = qobject_cast<KUrlRequester*>(formWidget)) {
            auto text = group.readEntry(name, QString {});
            widget->setText(text);
        } else if (auto widget = qobject_cast<KEditListWidget*>(formWidget)) {
            auto items = group.readEntry(name, QString {}).split(QLatin1Char(':'));
            widget->setItems(items);
        } else if (auto widget = qobject_cast<QComboBox*>(formWidget)) {
            auto value = group.readEntry(name, QString {});
            if (value.isEmpty()) {
                continue;
            }
            auto index = widget->findText(value);
            if (index == -1) {
                index = widget->count();
                widget->addItem(value);
            }
            widget->setCurrentIndex(index);
        } else {
            qWarning() << formWidget->metaObject()->className() << "is not supported in MultiConfigWidget";
        }
    }
}

void MultiConfigWidget::saveConfig(const QString& name)
{
    if (!m_child) {
        return;
    }

    if (name.isEmpty() || !m_group.isValid()) {
        return;
    }

    m_saving = true;

    auto group = m_group.group(name);

    for (const auto formWidget : std::as_const(m_formWidgets)) {
        const auto& name = formWidget->objectName();
        if (auto widget = qobject_cast<const QLineEdit*>(formWidget)) {
            group.writeEntry(name, widget->text());
        } else if (auto widget = qobject_cast<const KUrlRequester*>(formWidget)) {
            group.writeEntry(name, widget->text());
        } else if (auto widget = qobject_cast<const KEditListWidget*>(formWidget)) {
            auto data = widget->items().join(QLatin1Char(':'));
            group.writeEntry(name, data);
        } else if (auto widget = qobject_cast<const QComboBox*>(formWidget)) {
            group.writeEntry(name, widget->currentText());
        } else {
            qWarning() << formWidget->metaObject()->className() << "is not supported in MultiConfigWidget";
        }
    }

    m_saving = false;
}

void MultiConfigWidget::saveCurrentConfig()
{
    saveConfig(currentConfig());
}

QString MultiConfigWidget::currentConfig() const
{
    return m_configWidget->currentConfigComboBox->currentText();
}

QStringList MultiConfigWidget::configs() const
{
    auto configs = m_group.groupList();
    // KConfig is weird in regards to deleted groups
    // they are still in groupList but are no longer valid
    // TODO: C++20 use filter
    configs.erase(std::remove_if(configs.begin(), configs.end(),
                                 [group = m_group](const QString& groupName) {
                                     return !group.hasGroup(groupName) || !group.group(groupName).exists();
                                 }),
                  configs.end());
    return configs;
}
