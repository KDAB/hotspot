#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStringLiteral>


SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mainWindow = (MainWindow*)parent;
    ui->setupUi(this);

    ui->label1->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label2->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label3->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label4->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label5->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label6->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label7->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->label8->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    // Set text for each LineEdit from corresponding variables in MainWindow class instance
    ui->lineEditSysroot->setGrayedText(QLatin1String("local machine"));
    ui->lineEditSysroot->setToolTip(QLatin1String("Path to the sysroot. Leave empty to use the local machine."));
    ui->lineEditSysroot->setText(mainWindow->getSysroot());

    ui->lineEditApplicationPath->setToolTip(QLatin1String("Path to the application binary and library."));

    m_appPath = mainWindow->getApplicationPath();

    // If the value of property with Perf data path override checkbox state is "checked", then
    if (mainWindow->getOverrideAppPathWithPerfDataPath()) {
        // Check the checkbox
        ui->checkBoxOverrideWithPerfDataPath->setCheckState(Qt::Checked);
        ui->btnApplicationPath->setEnabled(false);
        ui->lineEditApplicationPath->setText(mainWindow->getPerfDataPath());
        ui->lineEditApplicationPath->setTextDisabled();
    } else {
        // Uncheck the checkbox
        ui->checkBoxOverrideWithPerfDataPath->setCheckState(Qt::Unchecked);
        ui->btnApplicationPath->setEnabled(true);
        ui->lineEditApplicationPath->setText(m_appPath);
        ui->lineEditApplicationPath->setTextEnabled();
    }

    ui->lineEditTargetRoot->setGrayedText(QLatin1String("auto-detect"));
    ui->lineEditTargetRoot->setToolTip(QLatin1String("Target machine path"));
    ui->lineEditTargetRoot->setText(mainWindow->getTargetRoot());

    ui->lineEditExtraLibraryPaths->setGrayedText(QLatin1String("empty"));
    ui->lineEditExtraLibraryPaths->setToolTip(QLatin1String("List of colon-separated paths that contain additional libraries."));
    ui->lineEditExtraLibraryPaths->setText(mainWindow->getExtraLibPaths());

    ui->lineEditDebugPaths->setGrayedText( QLatin1String("auto-detect"));
    ui->lineEditDebugPaths->setToolTip(QLatin1String("List of colon-separated paths that contain debug information."));
    ui->lineEditDebugPaths->setText(mainWindow->getDebugPaths());

    ui->lineEditKallsyms->setGrayedText(QLatin1String("auto-detect"));
    ui->lineEditKallsyms->setToolTip(QLatin1String("Path to the kernel symbol mapping."));
    ui->lineEditKallsyms->setText(mainWindow->getKallsyms());

    ui->comboBoxArchitecture->setToolTip(QLatin1String("System architecture, e.g. x86_64, arm, aarch64 etc."));
    // Restore the index of current item in combobox from MainWindow class instance
    int itemIndex = ui->comboBoxArchitecture->findText(mainWindow->getArch());
    // If index is -1 then arch from MainWindow class instance isn't in the combobox
    if (0 <= itemIndex && itemIndex < ui->comboBoxArchitecture->count())
        // Restore the text of current item in combobox by index
        ui->comboBoxArchitecture->setCurrentIndex(itemIndex);

    ui->comboBoxMaxStack->setValidator(new QIntValidator());
    ui->comboBoxMaxStack->setToolTip(QLatin1String("Maximum size of callchain and branchStack."));
    ui->comboBoxMaxStack->setCompleter(nullptr);

    QString maxStack = mainWindow->getMaxStack();
    if (!maxStack.isEmpty()) {
        if (maxStack == infinityValue) {
            maxStack = infinityText;
        }
        ui->comboBoxMaxStack->setCurrentText(maxStack);
    }
    connect(ui->comboBoxMaxStack, &QComboBox::currentTextChanged, this, [this]() {
        maxStackChanged = true;
    });
}

QString SettingsDialog::chooseDirectory()
{
    return QFileDialog::getExistingDirectory(nullptr, tr("Choose directory"), tr(""));
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

// Choose the directory for each LineEdit "Browse" button.
// If it was chosen ("Ok" pressed) then set it to LineEdit
// In else case ("Cancel" pressed) do not erase LineEdit text by an empty string
void SettingsDialog::on_btnSysroot_clicked() {
    chooseDirectoryAndCopyToLineEdit(ui->lineEditSysroot);
}
void SettingsDialog::on_btnApplicationPath_clicked() {
    chooseDirectoryAndCopyToLineEdit(ui->lineEditApplicationPath);
}
void SettingsDialog::on_btnTargetRoot_clicked() {
    chooseDirectoryAndCopyToLineEdit((ui->lineEditTargetRoot));
}
void SettingsDialog::on_btnExtraLibraryPaths_clicked() {
    chooseDirectoryAndAddToLineEdit(ui->lineEditExtraLibraryPaths);
}
void SettingsDialog::on_btnDebugPaths_clicked() {
    chooseDirectoryAndAddToLineEdit(ui->lineEditDebugPaths);
}
void SettingsDialog::on_btnKallsyms_clicked() {
    chooseDirectoryAndCopyToLineEdit(ui->lineEditKallsyms);
}

void SettingsDialog::on_checkBoxOverrideWithPerfDataPath_clicked() {
    if (ui->checkBoxOverrideWithPerfDataPath->isChecked()) {
        m_appPath = ui->lineEditApplicationPath->text();
        ui->lineEditApplicationPath->setText(mainWindow->getPerfDataPath());
        ui->lineEditApplicationPath->setTextDisabled();
        ui->btnApplicationPath->setDisabled(true);
    } else {
        ui->lineEditApplicationPath->setText(m_appPath);
        ui->lineEditApplicationPath->setTextEnabled();
        ui->btnApplicationPath->setEnabled(true);
    }
}

void SettingsDialog::chooseDirectoryAndCopyToLineEdit(QWebStyleEdit* qWebStyleEdit) {
    QString sDir = chooseDirectory();
    if (!sDir.isEmpty())
        qWebStyleEdit->setText(sDir);
}

void SettingsDialog::chooseDirectoryAndAddToLineEdit(QWebStyleEdit* qWebStyleEdit) {
    QString sDir = chooseDirectory();
    if (!sDir.isEmpty()) {
        QString sDirs = qWebStyleEdit->text();
        if (sDirs.isEmpty())
            sDirs = sDir;
        else
            sDirs = sDirs + tr(":") + sDir;
        qWebStyleEdit->setText(sDirs);
    }
}

void SettingsDialog::on_buttonBox_clicked(QAbstractButton * button) {
    // If "Ok" button pressed save each LineEdit text to MainWindow class instance variable
    if (button == ui->buttonBox->button(QDialogButtonBox::Ok)) {
        // Save LineEdit texts to the MainWindow class instance variables
        mainWindow->setSysroot(ui->lineEditSysroot->text());
        mainWindow->setAppPath(ui->lineEditApplicationPath->text());
        mainWindow->setExtraLibPaths(ui->lineEditExtraLibraryPaths->text());
        mainWindow->setDebugPaths(ui->lineEditDebugPaths->text());
        mainWindow->setKallsyms(ui->lineEditKallsyms->text());
        QString sArch = ui->comboBoxArchitecture->currentText();
        // If architecture is "auto-detect", then change it to an empty string
        sArch = sArch == QLatin1String("auto-detect") ? QLatin1String("") : sArch;
        // Save architecture to the MainWindow class instance variable
        mainWindow->setArch(sArch);
        mainWindow->setTargetRoot(ui->lineEditTargetRoot->text());
        mainWindow->setOverrideAppPathWithPerfDataPath(ui->checkBoxOverrideWithPerfDataPath->isChecked());

        if (ui->comboBoxMaxStack->lineEdit()->hasAcceptableInput()) {
            mainWindow->setMaxStack(ui->comboBoxMaxStack->currentText());
        } else if (ui->comboBoxMaxStack->currentText().compare(infinityText, Qt::CaseInsensitive) == 0) {
            mainWindow->setMaxStack(infinityValue);
        }

        if (maxStackChanged) {
            QMessageBox::information(this,
                                     QStringLiteral("Max stack is changed"),
                                     QStringLiteral("Reload to apply new max stack value")
                                     );
        }
    }
    close();
}
