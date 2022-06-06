/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsflamegraphpage.h"
#include "ui_resultsflamegraphpage.h"

#include "parsers/perf/perfparser.h"

#include <QAction>
#include <QFileDialog>
#include <QImageWriter>
#include <QMenu>
#include <QMessageBox>
#include <QTextStream>

namespace {
QString imageFormatFilter()
{
    QString filter;
    {
        QTextStream stream(&filter);
        const auto supportedFormats = QImageWriter::supportedImageFormats();
        for (const auto& format : supportedFormats)
            stream << "*." << format.toLower() << ' ';
    }
    filter.chop(1); // remove trailing whitespace
    return filter;
}
}

ResultsFlameGraphPage::ResultsFlameGraphPage(FilterAndZoomStack* filterStack, PerfParser* parser, QMenu* exportMenu,
                                             QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsFlameGraphPage)
{
    ui->setupUi(this);
    ui->flameGraph->setFilterStack(filterStack);

    connect(parser, &PerfParser::bottomUpDataAvailable, this, [this, exportMenu](const Data::BottomUpResults& data) {
        ui->flameGraph->setBottomUpData(data);
        m_exportAction = exportMenu->addAction(QIcon::fromTheme(QStringLiteral("image-x-generic")), tr("Flamegraph"));
        connect(m_exportAction, &QAction::triggered, this, [this]() {
            const auto filter = tr("Images (%1);;SVG (*.svg)").arg(imageFormatFilter());
            QString selectedFilter;
            const auto fileName =
                QFileDialog::getSaveFileName(this, tr("Export Flamegraph"), {}, filter, &selectedFilter);
            if (fileName.isEmpty())
                return;
            if (selectedFilter.contains(QStringLiteral("svg"))) {
                ui->flameGraph->saveSvg(fileName);
            } else {
                QImageWriter writer(fileName);
                if (!writer.write(ui->flameGraph->toImage())) {
                    QMessageBox::warning(this, tr("Export Failed"),
                                         tr("Failed to export flamegraph: %1").arg(writer.errorString()));
                }
            }
        });
    });

    connect(parser, &PerfParser::topDownDataAvailable, this,
            [this](const Data::TopDownResults& data) { ui->flameGraph->setTopDownData(data); });

    connect(ui->flameGraph, &FlameGraph::jumpToCallerCallee, this, &ResultsFlameGraphPage::jumpToCallerCallee);
    connect(ui->flameGraph, &FlameGraph::openEditor, this, &ResultsFlameGraphPage::openEditor);
    connect(ui->flameGraph, &FlameGraph::selectSymbol, this, &ResultsFlameGraphPage::selectSymbol);
    connect(ui->flameGraph, &FlameGraph::jumpToDisassembly, this, &ResultsFlameGraphPage::jumpToDisassembly);
}

ResultsFlameGraphPage::~ResultsFlameGraphPage() = default;

void ResultsFlameGraphPage::clear()
{
    ui->flameGraph->clear();
    delete m_exportAction;
    m_exportAction = nullptr;
}

void ResultsFlameGraphPage::setHoveredStacks(const QVector<QVector<Data::Symbol>>& hoveredStacks)
{
    ui->flameGraph->setHoveredStacks(hoveredStacks);
}
