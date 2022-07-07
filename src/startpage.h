/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

public slots:
    void onOpenFileError(const QString& errorMessage);
    void onParseFileProgress(float percent);
    void onDebugInfoDownloadProgress(const QString& url, qint64 numerator, qint64 denominator);

signals:
    void openFileButtonClicked();
    void recordButtonClicked();
    void stopParseButtonClicked();
    void pathSettingsButtonClicked();

private:
    void updateBackground();

    QScopedPointer<Ui::StartPage> ui;

    QPixmap m_background;
};
