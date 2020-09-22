#ifndef QWEBSTYLEEDIT_H
#define QWEBSTYLEEDIT_H

#include <QLineEdit>
#include <QColor>

class QWebStyleEdit : public QLineEdit
{
    Q_OBJECT
    Q_PROPERTY(QString grayedText READ grayedText WRITE setGrayedText)

    QString m_text;
    QString m_grText;
    const QColor textBGColor = palette().color(QPalette::Base);
    const QColor grTextBGColor = palette().color(QPalette::Base);
    const QColor textFGColor = palette().color(QPalette::Text);
    const QColor grTextFGColor = palette().color(QPalette::Midlight);

    bool modified;

    void setBackgroundColor(QColor);
    void setForegroundColor(QColor);
    void setItalic(bool);

public:
    QWebStyleEdit(QWidget* parent = 0, QString grText = QString());

    void setText(const QString&);
    QString text() const;
    void setGrayedText(const QString&);
    QString grayedText() const;

    void grTextFGColorsetText(const QString&);
    void setColors();
    void setTextEnabled();
    void setTextDisabled();
protected:
    void keyPressEvent(QKeyEvent*) override;
};

#endif // QWEBSTYLEEDIT_H
