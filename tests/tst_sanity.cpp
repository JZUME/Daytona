// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

// Confirms the Qt Test and CTest wiring works. Replace with real tests as
// components are added.
class TestSanity : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void versionIsDefined() { QVERIFY(QByteArray(DAYTONA_VERSION).size() > 0); }
};

QTEST_GUILESS_MAIN(TestSanity)
#include "tst_sanity.moc"
