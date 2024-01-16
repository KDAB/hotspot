/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    (void)argc;

    FILE* file;
    long fileSize;
    char* buffer;
    int i;

    for (i = 0; i < 500000; ++i) {
        file = fopen(argv[0], "rb");
        if (!file) {
            fprintf(stderr, "failed to open file %s\n", argv[0]);
            return 1;
        }

        fseek(file, 0, SEEK_END);
        fileSize = ftell(file);
        rewind(file);

        fclose(file);
    }

    (void)fileSize;
    (void)buffer;

    return 0;
}
