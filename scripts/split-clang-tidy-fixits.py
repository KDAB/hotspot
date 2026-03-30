#!/bin/env python3
# SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
# SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys
import yaml


srcDir = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + "/"


def fileOffsetToLine(message):
    ''' to ease manual inspection, translate FileOffset to a FileLine '''
    if message['FilePath'] == "":
        return
    if message['FilePath'].startswith("/usr"):
        return
    # github action dir to local dir
    message['FilePath'] = message['FilePath'].replace("/__w/hotspot/hotspot/", srcDir)
    # build dir files may not exist when downloading from github
    if "/build-" in message['FilePath']:
        return
    with open(message['FilePath'], 'r', encoding='utf-8') as sourceFile:
        numNewlines = sourceFile.read(message['FileOffset']).count('\n')
        message['FileLine'] = numNewlines + 1


inputFile = sys.argv[1]
groupedFixits = {}


with open(inputFile, 'r', encoding='utf-8') as mainFixitsFile:
    mainFixits = yaml.safe_load(mainFixitsFile)

    if not mainFixits:
        print("no diagnostics found")
        sys.exit(0)

    for fixit in mainFixits['Diagnostics']:
        fileOffsetToLine(fixit['DiagnosticMessage'])
        for note in fixit.get('Notes', []):
            fileOffsetToLine(note)

        diagnostic = fixit['DiagnosticName']
        group = groupedFixits.get(diagnostic)
        if not group:
            groupedFixits[diagnostic] = [fixit]
        else:
            group.append(fixit)


baseDir = os.path.dirname(inputFile)
if baseDir == "":
    baseDir = os.getcwd()

for diagnostic, fixits in groupedFixits.items():
    diagnosticDir = f"{baseDir}/{diagnostic}"
    if not os.path.isdir(diagnosticDir):
        os.mkdir(diagnosticDir)
    with open(f"{diagnosticDir}/fixits.yaml", 'w', encoding='utf-8') as fixitsFile:
        text = yaml.dump({'Diagnostics': fixits, 'MainSourceFile': ''})

        # sadly clang-apply-replacements doesn't like our additional FileLine
        # and we cannot add comments directly with pyaml
        # so instead we do this manually here
        text = text.replace('FileLine:', '# FileLine:')

        fixitsFile.write(text)
