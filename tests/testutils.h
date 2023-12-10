/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QTextStream>

#include <models/callercalleemodel.h>
#include <models/data.h>
#include <models/topproxy.h>
#include <models/treemodel.h>

#include <algorithm>

#define VERIFY_OR_THROW(statement)                                                                                     \
    do {                                                                                                               \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__))                         \
            throw std::logic_error("verify failed: " #statement);                                                      \
    } while (false)

#define VERIFY_OR_THROW2(statement, description)                                                                       \
    do {                                                                                                               \
        if (!QTest::qVerify(static_cast<bool>(statement), #statement, description, __FILE__, __LINE__))                \
            throw std::logic_error(description);                                                                       \
    } while (false)

#define COMPARE_OR_THROW(actual, expected)                                                                             \
    do {                                                                                                               \
        if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))                                \
            throw std::logic_error("compare failed: " #actual #expected);                                              \
    } while (false)

template<typename Data, typename Results>
QString printCost(const Data& node, const Results& results)
{
    return QLatin1String("s:") + QString::number(results.selfCosts.cost(0, node.id)) + QLatin1String(",i:")
        + QString::number(results.inclusiveCosts.cost(0, node.id));
}

inline QString printCost(const Data::BottomUp& node, const Data::BottomUpResults& results)
{
    QString ret;
    for (int i = 0; i < results.costs.numTypes(); ++i) {
        if (i != 0)
            ret += QLatin1String(", ");
        ret += QString::number(results.costs.cost(i, node.id));
    }
    return ret;
}

template<typename Tree, typename Results>
void printTree(const Tree& tree, const Results& results, QStringList* entries, int indentLevel)
{
    QString indent;
    indent.fill(QLatin1Char(' '), indentLevel);
    for (const auto& entry : tree.children) {
        entries->push_back(indent + entry.symbol.symbol() + QLatin1Char('=') + printCost(entry, results));
        printTree(entry, results, entries, indentLevel + 1);
    }
}

template<typename Results>
QStringList printTree(const Results& results)
{
    QStringList list;
    printTree(results.root, results, &list, 0);
    return list;
}

QStringView symbolSubString(const QString& string)
{
    auto idx = string.indexOf(QLatin1Char('>'));
    if (idx == -1) {
        idx = string.indexOf(QLatin1Char('<'));
    }
    if (idx == -1) {
        idx = string.indexOf(QLatin1Char('='));
    }
    return QStringView(string).mid(0, idx);
}

inline QStringList printMap(const Data::CallerCalleeResults& results)
{
    QStringList list;
    list.reserve(results.entries.size());
    QSet<quint32> ids;
    for (auto it = results.entries.begin(), end = results.entries.end(); it != end; ++it) {
        VERIFY_OR_THROW(!ids.contains(it->id));
        ids.insert(it->id);
        list.push_back(it.key().symbol() + QLatin1Char('=') + printCost(it.value(), results));
        QStringList subList;
        for (auto callersIt = it->callers.begin(), callersEnd = it->callers.end(); callersIt != callersEnd;
             ++callersIt) {
            subList.push_back(it.key().symbol() + QLatin1Char('<') + callersIt.key().symbol() + QLatin1Char('=')
                              + QString::number(callersIt.value()[0]));
        }
        for (auto calleesIt = it->callees.begin(), calleesEnd = it->callees.end(); calleesIt != calleesEnd;
             ++calleesIt) {
            subList.push_back(it.key().symbol() + QLatin1Char('>') + calleesIt.key().symbol() + QLatin1Char('=')
                              + QString::number(calleesIt.value()[0]));
        }
        subList.sort();
        list += subList;
    }
    std::stable_sort(list.begin(), list.end(), [](const QString& lhs, const QString& rhs) {
        return symbolSubString(lhs) < symbolSubString(rhs);
    });
    return list;
}

inline QStringList printCallerCalleeModel(const CallerCalleeModel& model)
{
    QStringList list;
    list.reserve(model.rowCount());
    for (int i = 0, c = model.rowCount(); i < c; ++i) {
        auto symbolIndex = model.index(i, CallerCalleeModel::Symbol);
        const auto symbol = symbolIndex.data().toString();
        const auto& selfCostIndex = model.index(i, CallerCalleeModel::Binary + 1);
        const auto& inclusiveCostIndex = model.index(i, CallerCalleeModel::Binary + 2);
        list.push_back(symbol + QLatin1String("=s:") + selfCostIndex.data(CallerCalleeModel::SortRole).toString()
                       + QLatin1String(",i:") + inclusiveCostIndex.data(CallerCalleeModel::SortRole).toString());
        QStringList subList;
        const auto& callers = symbolIndex.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
        for (auto callersIt = callers.begin(), callersEnd = callers.end(); callersIt != callersEnd; ++callersIt) {
            subList.push_back(symbol + QLatin1Char('<') + callersIt.key().symbol() + QLatin1Char('=')
                              + QString::number(callersIt.value()[0]));
        }
        const auto& callees = symbolIndex.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
        for (auto calleesIt = callees.begin(), calleesEnd = callees.end(); calleesIt != calleesEnd; ++calleesIt) {
            subList.push_back(symbol + QLatin1Char('>') + calleesIt.key().symbol() + QLatin1Char('=')
                              + QString::number(calleesIt.value()[0]));
        }
        subList.sort();
        list += subList;
    }
    std::stable_sort(list.begin(), list.end(), [](const QString& lhs, const QString& rhs) {
        return symbolSubString(lhs) < symbolSubString(rhs);
    });
    return list;
}

void dumpList(const QStringList& list)
{
    QTextStream out(stdout);
    for (const auto& line : list) {
        out << line << '\n';
    }
}

void printModelImpl(const QAbstractItemModel* model, const QModelIndex& parent, const QString& indent, QStringList* ret)
{
    for (int i = 0, c = model->rowCount(parent); i < c; ++i) {
        const auto index = model->index(i, 0, parent);
        ret->append(indent + index.data().toString());
        printModelImpl(model, index, indent + QLatin1String(" "), ret);
    }
}

QStringList printModel(const QAbstractItemModel* model)
{
    QStringList ret;
    printModelImpl(model, {}, {}, &ret);
    return ret;
}

inline QString findExe(const QString& name)
{
    QFileInfo exe(QCoreApplication::applicationDirPath() + QLatin1String("/../tests/test-clients/%1/%1").arg(name));
    VERIFY_OR_THROW(exe.exists() && exe.isExecutable());
    return exe.canonicalFilePath();
}

#define HOTSPOT_TEST_MAIN_IMPL(TestObject, QApp)                                                                       \
    int main(int argc, char** argv)                                                                                    \
    {                                                                                                                  \
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))                                                             \
            qputenv("QT_QPA_PLATFORM", "minimal");                                                                     \
                                                                                                                       \
        QApp app(argc, argv);                                                                                          \
        app.setAttribute(Qt::AA_Use96Dpi, true);                                                                       \
        TestObject tc;                                                                                                 \
        QTEST_SET_MAIN_SOURCE_PATH                                                                                     \
        QTest::qInit(&tc, argc, argv);                                                                                 \
        int ret = QTest::qRun();                                                                                       \
        QTest::qCleanup();                                                                                             \
        return ret;                                                                                                    \
    }

#define HOTSPOT_GUITEST_MAIN(TestObject) HOTSPOT_TEST_MAIN_IMPL(TestObject, QGuiApplication)
