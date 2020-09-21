#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include "qwebstyleedit.h"
#include "mainwindow.h"
#include "parsers/perf/perfparser.h"
#include "resultspage.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    MainWindow* mainWindow;
    explicit SettingsDialog(QWidget* parent);
    ~SettingsDialog();
    void setTexts(QWebStyleEdit, const QString&, const QString&, const QString&);
    QString chooseDirectory();

signals:
    void sysrootChanged(const QString& path);

private:
    Ui::SettingsDialog *ui;
    void chooseDirectoryAndCopyToLineEdit(QWebStyleEdit* qWebStyleEdit);

private slots:
    void on_btnSysroot_clicked();
    void on_btnApplicationPath_clicked();
    void on_btnExtraLibraryPaths_clicked();
    void on_btnDebugPaths_clicked();
    void on_btnKallsyms_clicked();
    void on_buttonBox_clicked(QAbstractButton* );
};

#endif // SETTINGSDIALOG_H
