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
    QColor textBGColor, grTextBGColor, textFGColor, grTextFGColor;
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
protected:
    // void focusInEvent(QFocusEvent*) override;
    // void focusOutEvent(QFocusEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
// protected slots:
//    void slot1();
};

#endif // QWEBSTYLEEDIT_H
