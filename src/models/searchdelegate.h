#pragma once

#include <QStyledItemDelegate>
#include "costdelegate.h"
#include "highlighter.h"

class SearchDelegate : public QStyledItemDelegate {
Q_OBJECT
public:
    SearchDelegate(QObject *parent = nullptr);

    ~SearchDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setSearchText(QString text) {
        m_searchText = text;
    }

    QString getSearchText() {
        return m_searchText;
    }

    void setArch(QString arch) {
        m_arch = arch;
    }

    QString getArch() {
        return m_arch;
    }

    const QModelIndexList getSelectedIndexes() const {
        return selectedIndexes;
    }

    void setSelectedIndexes(const QModelIndexList &selectedIndexes) {
        SearchDelegate::selectedIndexes = selectedIndexes;
    }

    void setDiagnosticStyle(bool diagnosticStyle) {
        m_diagnosticStyle = diagnosticStyle;
    }

    bool getDiagnosticStyle() {
        return m_diagnosticStyle;
    }

private:
    QString m_searchText;
    QModelIndexList selectedIndexes;
    CostDelegate *costDelegate;
    QString m_arch;
    bool m_diagnosticStyle;
};
