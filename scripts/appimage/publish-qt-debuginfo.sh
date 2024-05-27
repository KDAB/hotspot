#!/bin/sh
#
# SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
# SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -e

cd /workspace

gh release delete qt-debuginfo || true
gh release create qt-debuginfo /qt-debuginfo-x86_64.tar.bz2 --draft=false --notes "Qt and KDE debug symbols" --title "qt-kde-debuginfo" --latest=false
