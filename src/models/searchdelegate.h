#pragma once

#include <QStyledItemDelegate>
#include <QHash>
#include "costdelegate.h"
#include "highlighter.h"
#include "data.h"

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

    void setDiagnosticStyle(bool diagnosticStyle) {
        m_diagnosticStyle = diagnosticStyle;
    }

    bool getDiagnosticStyle() {
        return m_diagnosticStyle;
    }

    const QHash<int, Data::Symbol> getCallees() const {
        return m_callees;
    }

    void setCallees(const QHash<int, Data::Symbol> callees) {
        m_callees = callees;
    }

private:
    QString m_searchText;
    CostDelegate *costDelegate;
    QHash<int, Data::Symbol> m_callees;
    QString m_arch;
    bool m_diagnosticStyle;
};
