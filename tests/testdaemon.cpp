/*
 *   SPDX-FileCopyrightText: 2016 Arnav Dhamija <arnav.dhamija@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../src/iodaemon/stashnotifier.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    StashNotifier *stashDaemon = new StashNotifier(0, QVariantList());
    return app.exec();
}
