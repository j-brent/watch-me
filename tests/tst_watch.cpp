// Behaviour tests for statechart/watch.scxml. One check per spec row (T-xx),
// grouped by spec section. Spec: docs/STATECHART-SPEC.md
#include <QtTest>
#include "watch.h"

class TstWatch : public QObject
{
    Q_OBJECT
private slots:
    void topLevel();
};

void TstWatch::topLevel()
{
    WatchMachine m;
    m.start();
    QTRY_VERIFY(m.isActive(QStringLiteral("dead")));
    m.submitEvent(QStringLiteral("battery.inserted")); // T-01
    QTRY_VERIFY(m.isActive(QStringLiteral("alive")));
    m.submitEvent(QStringLiteral("battery.removed"));  // T-03
    QTRY_VERIFY(m.isActive(QStringLiteral("dead")));
}

QTEST_GUILESS_MAIN(TstWatch)
#include "tst_watch.moc"
