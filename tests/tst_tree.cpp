// ScxmlTreeModel (view data parsed from watch.scxml), TransitionTracer (event log)
// and SpecTags (confidence badges).
#include <QtTest>
#include "watch.h"
#include "ScxmlTreeModel.h"
#include "TransitionTracer.h"
#include "SpecTags.h"

class TstTree : public QObject
{
    Q_OBJECT
private:
    static void settle() { for (int i = 0; i < 5; ++i) QCoreApplication::processEvents(); }
private slots:
    void initTestCase() { QLoggingCategory::setFilterRules(QStringLiteral("scxml.statemachine.debug=false")); }
    void structure();
    void tracerLog();
    void specTags();
};

void TstTree::structure()
{
    ScxmlTreeModel t;
    QVERIFY(t.load(QStringLiteral(":/statechart/watch.scxml")));
    QVERIFY(t.errorString().isEmpty());
    const auto &n = t.nodes();
    QCOMPARE(n.value("watch").initial, QStringLiteral("dead"));
    QCOMPARE(n.value("alive").kind, QStringLiteral("parallel"));
    // the six orthogonal components of alive (Fig. 27), in document order
    QCOMPARE(n.value("alive").children, (QStringList{"main", "alarm1_status", "alarm2_status",
                                                     "chime_status", "light", "power"}));
    QCOMPARE(n.value("regular").kind, QStringLiteral("parallel"));
    QCOMPARE(n.value("regular").children, (QStringList{"timedate", "beep_test"}));
    QCOMPARE(n.value("displays_H").historyType, QStringLiteral("deep"));
    QCOMPARE(n.value("sw_H").historyType, QStringLiteral("deep"));
    QCOMPARE(n.value("alarm1_H").historyType, QStringLiteral("shallow"));
    QCOMPARE(n.value("time").kind, QStringLiteral("atomic"));
    QCOMPARE(n.value("update").kind, QStringLiteral("compound"));
    QCOMPARE(n.value("update").initial, QStringLiteral("update_sec"));
    QCOMPARE(t.ancestors("update_mon").first(), QStringLiteral("update"));
    // every transition has a spec id, and the history defaults are not transitions
    for (const auto &tr : t.transitions())
        QVERIFY2(!tr.specId.isEmpty(), qPrintable(tr.source + " " + tr.event));
    QCOMPARE(t.transitionsFrom("time").size(), 4);    // T-10 (x2), T-20, T-23
    QVERIFY(t.specIds().contains("T-83"));
}

void TstTree::tracerLog()
{
    WatchMachine m;
    ScxmlTreeModel tree;
    QVERIFY(tree.load(QStringLiteral(":/statechart/watch.scxml")));
    TransitionTracer tr(&m, &tree);
    m.start();
    settle();
    QVERIFY(tr.isActive("dead"));
    m.submitEvent("battery.inserted");
    settle();
    QVERIFY(tr.activeSet().value("time").toBool());
    QCOMPARE(tr.entries().last(), QStringLiteral("T-01 battery.inserted: dead → alive"));
    QVERIFY(tr.flashSet().contains("time"));
    m.submitEvent("a.down");
    settle();
    // first visit: hv_alarm1 is false -> the default branch fired, not the H branch
    QVERIFY(tr.entries().contains(QStringLiteral("T-10 a.down: time → alarm1")));
    m.submitEvent("a.up");
    settle();
    // one press at a time, as from the UI (the log is resolved per macrostep)
    for (int i = 0; i < 5; ++i) { m.submitEvent("a.down"); settle(); m.submitEvent("a.up"); settle(); }
    QVERIFY(m.isActive("alarm1"));
    QVERIFY(tr.entries().contains(QStringLiteral("T-10 a.down: time → alarm1_H")));
    QVERIFY(tr.firedSpecIds().contains("T-14"));
    tr.noteInput("x");
    QCOMPARE(tr.entries().last(), QStringLiteral("▶ x"));
}

void TstTree::specTags()
{
    SpecTags s;
    s.parse(QStringLiteral("| T-01 | a | b | c | [P] Fig. 1 |\n| T-02 | x | [A] |\n| T-03 | y | [F] |\nnot a row [A]\n"));
    QCOMPARE(s.tagFor("T-01"), QStringLiteral("P"));
    QCOMPARE(s.weakest({"T-01", "T-03"}), QStringLiteral("F"));
    QCOMPARE(s.weakest({"T-01", "T-02", "T-03"}), QStringLiteral("A"));
    QCOMPARE(s.weakest({"T-99"}), QString());
    // the real spec: every row tagged, all confirmed after Step 0
    SpecTags real;
    QVERIFY(real.load(QStringLiteral(":/docs/STATECHART-SPEC.md")));
    ScxmlTreeModel tree;
    tree.load(QStringLiteral(":/statechart/watch.scxml"));
    for (const auto &id : tree.specIds())
        QCOMPARE(real.tagFor(id), QStringLiteral("P"));
}

QTEST_GUILESS_MAIN(TstTree)
#include "tst_tree.moc"
