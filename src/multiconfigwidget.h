#ifndef MULTICONFIGWIDGET_H
#define MULTICONFIGWIDGET_H

#include <KConfigGroup>
#include <functional>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>

class MultiConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MultiConfigWidget(QWidget* parent = nullptr);

    void setSaveFunction(const std::function<void(KConfigGroup)>& saveFunction);
    void setRestoreFunction(const std::function<void(const KConfigGroup&)>& restoreFunction);
    QString currentConfig() const;

public slots:
    void setKConfigGroup(const KConfigGroup& group);
    void saveConfigAs(const QString& name);
    void updateCurrentConfig();
    void selectConfig(const QString& name);
    void restoreCurrent();

private slots:

private:
    std::function<void(KConfigGroup)> m_saveFunction;
    std::function<void(const KConfigGroup&)> m_restoreFunction;
    KConfigGroup m_config;
    QComboBox* m_comboBox;
    QPushButton* m_copyButton;
    QPushButton* m_removeButton;
};

#endif // MULTICONFIGWIDGET_H
