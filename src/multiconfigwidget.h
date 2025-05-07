/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
    void saveConfig(const KConfigGroup& group);
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
