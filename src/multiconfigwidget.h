/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022-2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KConfigGroup>
#include <QWidget>

namespace Ui {
class MultiConfigWidget;
}

/** this widget allows fast switching between different configurations
 * of m_child
 * saveConfig will automatically save all changes to m_group
 * use setChildWidget for an unparented one and moveWidgetToChild for a parented one
 * */
class MultiConfigWidget : public QWidget
{
    Q_OBJECT
public:
    MultiConfigWidget(QWidget* parent = nullptr);
    ~MultiConfigWidget() override;

    /** widget is the widget containing the form which will be shown inside the MultiConfigWidget
     * formWidgets is a list of all user editable widgets in widget that will automatically be saved and restored
     * each widget in formWidgets needs a unique name **/
    void setChildWidget(QWidget* widget, const QVector<QWidget*>& formWidgets);

    /** set group where everything should be saved in **/
    void setConfigGroup(const KConfigGroup& group);

    QString currentConfig() const;

public slots:
    void loadConfig(const QString& name);
    void saveConfig(const QString& name);
    void saveCurrentConfig();

signals:
    void configsChanged();
    void currentConfigChanged();

private:
    QStringList configs() const;
    std::unique_ptr<Ui::MultiConfigWidget> m_configWidget;
    QWidget* m_child = nullptr;
    QVector<QWidget*> m_formWidgets;
    KConfigGroup m_group;
    bool m_saving = false;
};
