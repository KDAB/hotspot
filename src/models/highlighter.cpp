#include "highlighter.h"

Highlighter::Highlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent),
          m_callee(false) {
    HighlightingRule rule;

    QColor greenColor(60, 138, 103);
    registersFormat.setForeground(greenColor);
    const QString registersPatterns[] = {
            QLatin1String("\\b[a-z]{2,4}\\b"),
            QLatin1String("\\b[a-z]{1,3}[0-9]{1,2}\\b"),
            QLatin1String("\\b[a-z]{1,3}[0-9]{1,2}[a-z]\\b")
    };
    for (const QString &pattern : registersPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = registersFormat;
        regHighlightingRules.append(rule);
    }

    QColor brickColor(153, 0, 0);
    offsetFormat.setForeground(brickColor);
    const QString offsetPatterns[] = {
            QLatin1String("\\b0x[a-z0-9]+\\b")
    };
    for (const QString &pattern : offsetPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = offsetFormat;
        offsetHighlightingRules.append(rule);
    }
}

/**
 *   Highlight with different colors Disassembly text on tab Disassembly
 * @param text
 */
void Highlighter::highlightBlock(const QString &text) {
    QStringList argList = text.trimmed().split(QLatin1String(","));
    QStringList arg1, arg2;
    QList<int> offsetList;
    QStringList argListFirst;

    HighlightingRule rule;
    searchFormat.setForeground(Qt::white);
    searchFormat.setBackground(m_highlightColor);
    const QString searchPatterns[] = {
        QRegularExpression::escape(m_searchText)
    };
    for (const QString &pattern : searchPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = searchFormat;
        searchHighlightingRules.append(rule);
    }

    QString opCodeCall = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String("bl") : QLatin1String("call");
    QColor magentaColor(153, 0, 153);
    callFormat.setForeground((m_callee) ? Qt::blue : magentaColor);

    const QString callPatterns[] = {
            (m_callee) ? opCodeCall.append(QLatin1String("q{0,1}\\s*[a-z0-9]+\\s*<")) : QLatin1String("[a-z0-9]+\\s*<"),
            QLatin1String("\\>")
    };

    for (const QString &pattern : callPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = callFormat;
        callHighlightingRules.append(rule);
    }

    if (!m_diagnosticStyle) {
        QString commentSymbol = m_arch.startsWith(QLatin1String("arm")) ? QLatin1String(";") : QLatin1String("#");
        if (m_arch.startsWith(QLatin1String("armv8"))) {
            commentSymbol = QLatin1String("//");
        }
        commentFormat.setForeground(Qt::gray);
        const QString commentPatterns[] = {
                commentSymbol + QLatin1String(".*")
        };
        for (const QString &pattern : commentPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = commentFormat;
            commentHighlightingRules.append(rule);
        }

        if (m_arch.startsWith(QLatin1String("arm"))) {
            const QString offsetPatterns[] = {
                    QLatin1String("#[-0-9]+")
            };
            for (const QString &pattern : offsetPatterns) {
                rule.pattern = QRegularExpression(pattern);
                rule.format = offsetFormat;
                offsetHighlightingRules.append(rule);
            }
        }

        argListFirst = argList[0].trimmed().split(QRegExp(QLatin1String("[a-z]+\\s")));

        if (argListFirst.size() > 1) {
            argList[0] = argListFirst[argListFirst.size() - 1];

            for (int i = 0; i < argList.size(); i++) {
                if (argList[i].isEmpty())
                    continue;
                argList[i] = argList[i].trimmed();
                offsetList.push_back(text.indexOf(argList[i]) + argList[i].indexOf(QRegExp(QLatin1String("["))) +
                                     argList[i].indexOf(QRegExp(QLatin1String("("))));

                for (const HighlightingRule &rule : qAsConst(regHighlightingRules)) {
                    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
                    while (matchIterator.hasNext()) {
                        QRegularExpressionMatch match = matchIterator.next();
                        if (match.capturedStart() >= offsetList[i]) {
                            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
                        }
                    }
                }

                for (const HighlightingRule &rule : qAsConst(offsetHighlightingRules)) {
                    QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
                    while (matchIterator.hasNext()) {
                        QRegularExpressionMatch match = matchIterator.next();
                        if (match.capturedStart() >= offsetList[i]) {
                            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
                        }
                    }
                }
            }
        }
        if (!argListFirst.isEmpty()) {
            int plusOffset = text.lastIndexOf(QLatin1String("+"));
            int parenthesisOffset = (plusOffset > 0) ? plusOffset : 1 + text.lastIndexOf(QLatin1String(">"));
            for (const HighlightingRule &rule : qAsConst(callHighlightingRules)) {
                QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
                while (matchIterator.hasNext()) {
                    QRegularExpressionMatch match = matchIterator.next();
                    int startOffset = match.capturedStart();
                    setFormat(startOffset, (startOffset < parenthesisOffset) ?
                                           (parenthesisOffset - startOffset) : match.capturedLength(),
                              rule.format);
                }
            }
        }

        for (const HighlightingRule &rule : qAsConst(commentHighlightingRules)) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }
    for (const HighlightingRule &rule : qAsConst(searchHighlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    setCurrentBlockState(0);
}
