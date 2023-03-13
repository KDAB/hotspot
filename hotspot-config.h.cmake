/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#define HOTSPOT_VERSION_STRING "@HOTSPOT_VERSION_STRING@"
#define HOTSPOT_VERSION_MAJOR @hotspot_VERSION_MAJOR@
#define HOTSPOT_VERSION_MINOR @hotspot_VERSION_MINOR@
#define HOTSPOT_VERSION_PATCH @hotspot_VERSION_PATCH@
#define HOTSPOT_VERSION ((hotspot_VERSION_MAJOR<<16)|(hotspot_VERSION_MINOR<<8)|(hotspot_VERSION_PATCH))

#define HOTSPOT_LIBEXEC_REL_PATH "@LIBEXEC_REL_PATH@"

#cmakedefine01 APPIMAGE_BUILD

#cmakedefine01 Zstd_FOUND

#cmakedefine01 KF5Auth_FOUND

#cmakedefine01 KF5Archive_FOUND

#cmakedefine01 QCustomPlot_FOUND

#cmakedefine01 KGraphViewerPart_FOUND

#cmakedefine01 KF5SyntaxHighlighting_FOUND

#cmakedefine01 ALLOW_PRIVILEGE_ESCALATION
