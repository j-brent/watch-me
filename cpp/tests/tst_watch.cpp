// Behaviour tests for statechart/watch.scxml (generated class WatchMachine).
// One test function per spec section; every T-xx row of docs/STATECHART-SPEC.md is
// checked at least once (tests/conformance.py enforces that each row is cited here).
//
// Each check verifies the resulting configuration *and* that the transition
// labelled with the row (<log label="T-xx"/>) actually fired.
//
// Time is explicit: the statechart's "n sec in X" timers run on TimeoutService's
// clock, which only moves when a test calls advance(). Timing checks are exact
// (119.999 s: still in date; 120 s: back in time) and independent of machine speed.
#include <QtTest>
#include <QScxmlEvent>
#include "watch.h"
#include "TimeoutService.h"

namespace {

// Qt SCXML reports datamodel/executable-content errors (error.execution) only on the
// qt.scxml.statemachine debug category. Collect them; cleanup() fails on any.
QStringList g_scxmlErrors;
QtMessageHandler g_prevHandler = nullptr;
void scxmlErrorCatcher(QtMsgType t, const QMessageLogContext &ctx, const QString &msg)
{
    if (msg.contains(QLatin1String("had error")))
        g_scxmlErrors << msg;
    if (qstrcmp(ctx.category, "qt.scxml.statemachine") == 0)
        return; // very chatty: keep the test output readable
    g_prevHandler(t, ctx, msg);
}

constexpr qint64 kSec = 1000;
constexpr qint64 kMin = 60 * kSec;

class Watch
{
public:
    explicit Watch(bool anomalies = true) : t(&m)
    {
        // https://doc.qt.io/qt-6/qscxmlstatemachine.html#initialValues-prop
        m.setInitialValues({{QStringLiteral("harelAnomalies"), anomalies}});
        QObject::connect(&m, &QScxmlStateMachine::log, &m,
                         [this](const QString &label, const QString &) { fired << label; });
        // Outgoing qt:signal events (spec §3.3/§3.5): record name + field param.
        // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent
        for (const char *e : {"model.increment", "model.reset", "sw.start", "sw.stop", "sw.reset",
                              "sw.lap", "beep.short"}) {
            m.connectToEvent(QString::fromLatin1(e), &m, [this](const QScxmlEvent &ev) {
                const QString field = ev.data().toMap().value(QStringLiteral("field")).toString();
                out << (field.isEmpty() ? ev.name() : ev.name() + QLatin1Char(':') + field);
            });
        }
        m.start();
        settle();
    }

    void settle()
    {
        for (int i = 0; i < 5; ++i)
            QCoreApplication::processEvents();
    }
    void send(const QString &e)
    {
        m.submitEvent(e);
        settle();
    }
    void down(char b) { send(QStringLiteral("%1.down").arg(QLatin1Char(b))); }
    void up(char b) { send(QStringLiteral("%1.up").arg(QLatin1Char(b))); }
    void click(char b) { down(b); up(b); }
    void clicks(const char *bs) { for (; *bs; ++bs) click(*bs); }
    bool in(const char *s) const { return m.isActive(QString::fromLatin1(s)); }
    bool did(const char *t) const { return fired.contains(QString::fromLatin1(t)); }
    void reset() { fired.clear(); out.clear(); }
    void setAlarms(int t1, int t2)
    {
        // the host boundary (DESIGN-NOTES D-24), as WatchModel does it
        m.submitEvent(QStringLiteral("host.alarms"),
                      QVariantMap{{QStringLiteral("T1"), t1}, {QStringLiteral("T2"), t2}});
        settle();
    }

    void alive() { send(QStringLiteral("battery.inserted")); }

    // Move the timer clock forward by ms (fires every timeout reached) and let the
    // machine process the resulting events.
    void advance(qint64 ms) { t.advance(ms); settle(); }
    // From time: hold c for 2 s (wait -> update.sec), release.
    void enterUpdate() { down('c'); advance(2 * kSec); up('c'); }

    WatchMachine m;
    TimeoutService t;                        // after m: it connects to m's events
    QStringList fired;
    QStringList out;
};

} // namespace

// `fired` accumulates until Watch::reset(): call reset() before the step whose
// transition FIRED is meant to check, or an earlier firing satisfies it.
#define FIRED(w, t) QVERIFY2((w).did(t), "expected " t " to fire")
#define NOT_FIRED(w, t) QVERIFY2(!(w).did(t), t " must not fire")
// For a spec row that has no <log> of its own (T-15: no transition; T-56: actions
// of T-50/T-51/T-52), the assertion that checks it. tests/conformance.py counts
// FIRED for rows with a log and ROW_CHECK for rows without one; comments don't count.
#define ROW_CHECK(t, cond) QVERIFY2((cond), t ": " #cond)

class TstWatch : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("qt.scxml.statemachine.debug=true\n"
                                                        "scxml.statemachine.debug=false"));
        g_prevHandler = qInstallMessageHandler(scxmlErrorCatcher);
    }
    void cleanup()
    {
        const QStringList errors = std::exchange(g_scxmlErrors, {});
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
    }
    void topLevel();
    void displayCycle();
    void timeDateUpdate();
    void updateTimeout();
    void dateTimeout();
    void idleRestart();
    void idleRestartEveryButton_data();
    void idleRestartEveryButton();
    void alarmDisplays();
    void outTimeout();
    void stopwatch();
    void alarmsBeep();
    void chimeLightBeepTest();
    void harelAnomalyFlagOff();

    // conditions, history and unexpected input (test review, MC-1..MC-10)
    void alarmConditionsEqualTimes();
    void displaysRememberOn();
    void historyForgottenAfterRemoval();
    void alarmEndConsumesEveryButton_data();
    void alarmEndConsumesEveryButton();
    void unexpectedEvents();

    // scenario tests required by PLAN.md P3 (+ the paper's own scenario, p. 250)
    void scenarioLightAndExitUpdate();
    void scenarioAlarmInStopwatchLap();
    void scenarioBothBeepOnly();
    void scenarioAlarmTimeoutReturnsToPrevious();
    void scenarioBatteryDiesInAlarm2Update();
    void scenarioBeepTestFromDate();
    void scenarioStopwatchKeepsRunning();
    void scenarioPaperMonDThenB();
};

// ---------------------------------------------------------------- §3.1 top level
void TstWatch::topLevel()
{
    Watch w;
    QVERIFY(w.in("dead"));                  // initial state [P] Fig. 26

    w.alive();                              // T-01
    FIRED(w, "T-01");
    QVERIFY(w.in("alive"));
    for (auto s : {"time", "bt_00", "alarm1_disabled", "alarm2_disabled", "chime_disabled",
                   "light_off", "power_ok"})
        QVERIFY2(w.in(s), s);
    QVERIFY(w.out.contains("model.reset"));

    w.send("battery.dies");                 // only from power.blink
    QVERIFY(w.in("alive"));
    NOT_FIRED(w, "T-02");

    w.send("battery.weakens");              // T-04
    FIRED(w, "T-04");
    QVERIFY(w.in("power_blink"));

    w.send("battery.dies");                 // T-02
    FIRED(w, "T-02");
    QVERIFY(w.in("dead"));
    QVERIFY(!w.in("alive"));

    w.alive();
    QVERIFY(w.in("power_ok"));              // power re-entered by default
    w.reset();
    w.send("battery.removed");              // T-03
    FIRED(w, "T-03");
    QVERIFY(w.in("dead"));
    w.send("battery.removed");              // nothing in dead
    QVERIFY(w.in("dead"));
}

// -------------------------------------------------------------- §3.2 display cycle
void TstWatch::displayCycle()
{
    Watch w;
    w.alive();
    w.click('a');                            // T-10
    FIRED(w, "T-10");
    QVERIFY(w.in("alarm1") && w.in("alarm1_off") && w.in("out"));
    w.click('a');                            // T-11
    FIRED(w, "T-11");
    QVERIFY(w.in("alarm2") && w.in("alarm2_off"));
    w.click('a');                            // T-12
    FIRED(w, "T-12");
    QVERIFY(w.in("chime") && w.in("chime_off"));
    w.click('a');                            // T-13
    FIRED(w, "T-13");
    QVERIFY(w.in("stopwatch") && w.in("sw_zero"));
    QVERIFY(!w.in("out"));
    w.click('a');                            // T-14
    FIRED(w, "T-14");
    QVERIFY(w.in("time") && w.in("regular") && w.in("bt_00"));

    // T-15: a does nothing in date, update.*, update1.*, update2.*, wait
    w.click('d');
    QVERIFY(w.in("date"));
    w.reset();
    w.click('a');
    ROW_CHECK("T-15", w.in("date"));
    QVERIFY(w.fired == QStringList{"T-47"}); // only the 2-min timer restart

    w.click('d');
    QVERIFY(w.in("time"));
    w.down('c');                             // wait
    QVERIFY(w.in("wait"));
    w.click('a');
    ROW_CHECK("T-15", w.in("wait"));
    w.up('c');
    QVERIFY(w.in("time"));

    w.enterUpdate();
    QVERIFY(w.in("update_sec"));
    w.click('a');
    ROW_CHECK("T-15", w.in("update_sec"));
    w.click('b');
    QVERIFY(w.in("time"));

    w.click('a');
    w.click('c');                            // alarm1 -> update1
    QVERIFY(w.in("update1_hr"));
    w.click('a');
    ROW_CHECK("T-15", w.in("update1_hr"));
    w.click('b');
    w.click('a');
    w.click('c');
    QVERIFY(w.in("update2_hr"));
    w.click('a');
    ROW_CHECK("T-15", w.in("update2_hr"));
}

// ------------------------------------------------------ §3.3 time / date / update
void TstWatch::timeDateUpdate()
{
    Watch w;
    w.alive();

    w.click('d');                            // T-20 (b up)
    FIRED(w, "T-20");
    QVERIFY(w.in("date"));
    w.click('d');                            // T-21
    FIRED(w, "T-21");
    QVERIFY(w.in("time"));

    // T-20 needs "b up": with b held, d does not go to date
    w.reset();
    w.down('b');
    w.down('d');
    NOT_FIRED(w, "T-20");
    QVERIFY(w.in("time"));
    w.up('d');
    w.up('b');

    w.down('c');                             // T-23
    FIRED(w, "T-23");
    QVERIFY(w.in("wait"));
    QVERIFY(!w.in("regular"));               // wait is outside regular (no beep-test)
    w.advance(2 * kSec - 1);                 // released just before 2 s ...
    QVERIFY(w.in("wait"));
    w.up('c');                               // T-24 ... returns to time
    FIRED(w, "T-24");
    QVERIFY(w.in("time"));
    w.advance(10 * kSec);                    // and the cancelled wait timer stays dead
    QVERIFY(w.in("time"));
}

void TstWatch::updateTimeout()
{
    Watch w;
    w.alive();
    w.down('c');
    w.advance(2 * kSec - 1);
    QVERIFY(w.in("wait"));
    NOT_FIRED(w, "T-25");
    w.advance(1);                            // T-25 exactly at 2 s in wait
    FIRED(w, "T-25");
    QVERIFY(w.in("update_sec"));
    w.up('c');

    // T-26: sec -> 1min -> 10min -> hr -> mon -> date -> day -> year -> mode -> time
    const char *chain[] = {"update_1min", "update_10min", "update_hr", "update_mon",
                           "update_date", "update_day", "update_year", "update_mode"};
    w.reset();
    for (const char *s : chain) {
        // T-27: d increments the current field and stays in it
        const char *before = nullptr;
        for (const char *b : {"update_sec", "update_1min", "update_10min", "update_hr", "update_mon",
                              "update_date", "update_day", "update_year", "update_mode"})
            if (w.in(b)) before = b;
        QVERIFY(before);
        w.reset();                           // FIRED below must see this press, not an earlier one
        w.click('d');
        FIRED(w, "T-27");
        QVERIFY(w.in(before));
        QCOMPARE(w.out, QStringList{QStringLiteral("model.increment:%1").arg(QLatin1String(before))});
        w.reset();
        w.click('c');
        FIRED(w, "T-26");
        QVERIFY2(w.in(s), s);
    }
    w.click('c');                            // T-26: mode -c-> time
    QVERIFY(w.in("time"));
    QVERIFY(!w.in("update"));

    // T-28: b leaves update from any field
    w.enterUpdate();
    w.clicks("ccc");
    QVERIFY(w.in("update_hr"));
    w.reset();
    w.click('b');
    FIRED(w, "T-28");
    QVERIFY(w.in("time"));

    // T-29: 2 min in update with no button -> time (the c press restarted it)
    w.enterUpdate();
    w.click('c');
    QVERIFY(w.in("update_1min"));
    w.reset();
    w.advance(2 * kMin - 1);
    QVERIFY(w.in("update_1min"));
    w.advance(1);
    FIRED(w, "T-29");
    QVERIFY(w.in("time"));

    // Chained timeouts in one step (DESIGN-NOTES D-21): T-25 at 2 s arms update's
    // 2-min timer at 2 s, so T-29 fires at 122 s within the same advance().
    w.down('c');
    w.reset();
    w.advance(2 * kSec + 2 * kMin - 1);
    FIRED(w, "T-25");
    NOT_FIRED(w, "T-29");
    QVERIFY(w.in("update_sec"));
    w.advance(1);
    FIRED(w, "T-29");
    QVERIFY(w.in("time"));
    w.up('c');
    w.down('c');
    w.reset();
    w.advance(2 * kSec + 2 * kMin);          // the whole chain in a single step
    FIRED(w, "T-25");
    FIRED(w, "T-29");
    QVERIFY(w.in("time"));
    w.up('c');
}

void TstWatch::dateTimeout()
{
    Watch w;
    w.alive();
    w.click('d');
    QVERIFY(w.in("date"));
    QCOMPARE(w.t.remaining("date"), 2 * kMin);      // armed on entry
    w.advance(2 * kMin - 1);
    QVERIFY(w.in("date"));
    NOT_FIRED(w, "T-22");
    w.advance(1);                                    // T-22 exactly at 2 min
    FIRED(w, "T-22");
    QVERIFY(w.in("time"));
    QCOMPARE(w.t.remaining("date"), -1);
}

// T-47: any button restarts the 2-min timer (prose p. 247: "on condition that no
// button has been depressed in the interim"). Every button that keeps the watch in
// the state is tried: the event list has four entries and each can be wrong.
void TstWatch::idleRestartEveryButton_data()
{
    QTest::addColumn<QString>("where");
    QTest::addColumn<char>("button");
    for (const auto &[where, buttons] : {std::pair{"date", "abc"}, std::pair{"out", "bcd"},
                                         std::pair{"update", "acd"}})
        for (const char *b = buttons; *b; ++b)
            QTest::addRow("%s + %c", where, *b) << QString::fromLatin1(where) << *b;
}

void TstWatch::idleRestartEveryButton()
{
    QFETCH(QString, where);
    QFETCH(char, button);
    const QByteArray state = where.toLatin1();
    Watch w;
    w.alive();
    if (where == u"date") w.click('d');
    if (where == u"out") w.click('a');
    if (where == u"update") w.enterUpdate();
    QVERIFY(w.in(state));
    w.advance(100 * kSec);
    w.reset();
    w.click(button);
    FIRED(w, "T-47");
    QVERIFY(w.in(state));
    w.advance(2 * kMin - 1);                  // 220 s after entry, just under 2 min after the press
    QVERIFY(w.in(state));
    w.advance(1);
    QVERIFY(w.in("time"));
}

void TstWatch::idleRestart()
{
    // Leaving date cancels its timer, and a timeout from an earlier visit is
    // ignored after re-entry (timer generations).
    Watch w;
    w.alive();
    w.click('d');                             // date, first generation
    w.advance(100 * kSec);
    w.click('d');                             // leaves date (T-21); T-47 must not re-arm it
    QVERIFY(w.in("time"));
    QCOMPARE(w.t.remaining("date"), -1);
    w.advance(10 * kSec);
    w.click('d');                             // back into date: fresh 2-min timer
    w.advance(2 * kMin - 1);
    QVERIFY(w.in("date"));
    // a late timeout of an old generation (e.g. queued before the exit) is ignored
    w.m.submitEvent(QStringLiteral("timeout.date"), QVariantMap{{QStringLiteral("gen"), 1}});
    w.settle();
    QVERIFY(w.in("date"));
    w.advance(1);
    QVERIFY(w.in("time"));
}

// -------------------------------------------------------- §3.4 alarm / chime displays
void TstWatch::alarmDisplays()
{
    Watch w;
    w.alive();
    w.click('a');
    QVERIFY(w.in("alarm1_off") && w.in("alarm1_disabled"));

    w.click('d');                            // T-44 + T-41 in lockstep
    FIRED(w, "T-44");
    FIRED(w, "T-41");
    QVERIFY(w.in("alarm1_on") && w.in("alarm1_enabled"));
    w.reset();
    w.click('d');
    FIRED(w, "T-44");
    FIRED(w, "T-41");
    QVERIFY(w.in("alarm1_off") && w.in("alarm1_disabled"));
    w.click('d');
    QVERIFY(w.in("alarm1_on") && w.in("alarm1_enabled"));

    w.click('c');                            // T-30
    FIRED(w, "T-30");
    QVERIFY(w.in("update1_hr"));
    w.out.clear();
    w.click('d');                            // T-33
    FIRED(w, "T-33");
    QVERIFY(w.in("update1_hr"));
    QVERIFY(w.in("alarm1_enabled"));         // d in update1 does not touch the status
    QCOMPARE(w.out, QStringList{"model.increment:update1_hr"});
    w.click('c');                            // T-31
    FIRED(w, "T-31");
    QVERIFY(w.in("update1_10min"));
    w.out.clear();
    w.click('d');
    QCOMPARE(w.out, QStringList{"model.increment:update1_10min"});
    w.click('c');
    QVERIFY(w.in("update1_1min"));
    w.out.clear();
    w.click('d');
    QCOMPARE(w.out, QStringList{"model.increment:update1_1min"});
    w.click('c');                            // T-32 back to alarm1 via H -> on
    FIRED(w, "T-32");
    QVERIFY(w.in("alarm1_on"));
    w.click('c');
    w.click('c');
    QVERIFY(w.in("update1_10min"));
    w.reset();
    w.click('b');                            // T-34 back to alarm1 via H
    FIRED(w, "T-34");
    QVERIFY(w.in("alarm1_on"));
    QVERIFY(w.in("light_on") == false);      // b released again by click

    // alarm2: T-35..T-39, T-45, T-42
    w.click('a');
    QVERIFY(w.in("alarm2_off"));
    w.click('d');                            // T-45 + T-42
    FIRED(w, "T-45");
    FIRED(w, "T-42");
    QVERIFY(w.in("alarm2_on") && w.in("alarm2_enabled"));
    w.click('c');                            // T-35
    FIRED(w, "T-35");
    QVERIFY(w.in("update2_hr"));
    w.out.clear();
    w.click('d');                            // T-38
    FIRED(w, "T-38");
    QCOMPARE(w.out, QStringList{"model.increment:update2_hr"});
    w.click('c');                            // T-36
    FIRED(w, "T-36");
    QVERIFY(w.in("update2_10min"));
    w.click('c');
    QVERIFY(w.in("update2_1min"));
    w.click('c');                            // T-37
    FIRED(w, "T-37");
    QVERIFY(w.in("alarm2_on"));
    w.click('c');
    w.click('b');                            // T-39
    FIRED(w, "T-39");
    QVERIFY(w.in("alarm2_on"));
    w.click('d');
    QVERIFY(w.in("alarm2_off") && w.in("alarm2_disabled"));

    // chime: T-46 + T-43
    w.click('a');
    QVERIFY(w.in("chime_off") && w.in("chime_disabled"));
    w.click('d');
    FIRED(w, "T-46");
    FIRED(w, "T-43");
    QVERIFY(w.in("chime_on") && w.in("chime_enabled") && w.in("chime_quiet"));
    w.reset();
    w.click('d');
    FIRED(w, "T-46");
    FIRED(w, "T-43");
    QVERIFY(w.in("chime_off") && w.in("chime_disabled"));
    w.click('d');

    // history of the alarm/chime displays survives a full cycle (T-10/T-11/T-12 via H)
    w.clicks("aa");                          // stopwatch, time
    QVERIFY(w.in("time"));
    w.click('a');
    QVERIFY(w.in("alarm1_on"));
    w.click('a');
    QVERIFY(w.in("alarm2_off"));
    w.click('a');
    QVERIFY(w.in("chime_on"));

    // d outside the alarm displays does not change any status
    w.clicks("aa");
    QVERIFY(w.in("time"));
    w.click('d');
    w.click('d');
    QVERIFY(w.in("alarm1_enabled") && w.in("alarm2_disabled") && w.in("chime_enabled"));
}

void TstWatch::outTimeout()
{
    Watch w;
    w.alive();
    w.click('a');
    w.click('c');                            // update1 is inside out as well
    QVERIFY(w.in("update1_hr"));
    w.reset();
    w.advance(2 * kMin - 1);
    QVERIFY(w.in("update1_hr"));
    w.advance(1);                            // T-40
    FIRED(w, "T-40");
    QVERIFY(w.in("time"));
    QVERIFY(w.in("bt_00"));                  // entered regular by default

    // stopwatch is not in out: no automatic return
    Watch s;
    s.alive();
    s.clicks("aaaa");
    QVERIFY(s.in("stopwatch"));
    s.advance(60 * kMin);                    // no automatic return from the stopwatch
    QVERIFY(s.in("stopwatch"));
}

// ----------------------------------------------------------------- §3.5 stopwatch
void TstWatch::stopwatch()
{
    Watch w;
    w.alive();
    w.clicks("aaaa");
    QVERIFY(w.in("sw_zero"));

    w.out.clear();
    w.down('b');                             // T-50 (+ T-56 beep)
    FIRED(w, "T-50");
    QVERIFY(w.in("sw_nonzero") && w.in("sw_reg") && w.in("sw_on"));
    ROW_CHECK("T-56", (w.out == QStringList{"sw.start", "beep.short"}));
    QVERIFY(w.in("light_on"));               // b also lights the display (orthogonal)
    w.up('b');

    w.click('d');                            // T-53
    FIRED(w, "T-53");
    QVERIFY(w.in("sw_lap") && w.in("sw_on"));
    QVERIFY(w.out.contains("sw.lap"));
    w.click('d');                            // T-54
    FIRED(w, "T-54");
    QVERIFY(w.in("sw_reg"));

    w.out.clear();
    w.click('b');                            // T-51 (+ T-56)
    FIRED(w, "T-51");
    QVERIFY(w.in("sw_off"));
    ROW_CHECK("T-56", (w.out == QStringList{"sw.stop", "beep.short"}));

    w.out.clear();
    w.click('b');                            // T-52 (+ T-56)
    FIRED(w, "T-52");
    QVERIFY(w.in("sw_on"));
    ROW_CHECK("T-56", (w.out == QStringList{"sw.start", "beep.short"}));

    // d in reg while running -> lap (not zero); lap + stop; d in lap -> reg
    w.click('d');
    QVERIFY(w.in("sw_lap"));
    w.click('b');
    QVERIFY(w.in("sw_lap") && w.in("sw_off"));
    w.reset();
    w.click('d');                            // lap -> reg, not zero
    NOT_FIRED(w, "T-55");
    QVERIFY(w.in("sw_reg") && w.in("sw_off"));
    w.out.clear();
    w.click('d');                            // T-55 reg (in off) -> zero
    FIRED(w, "T-55");
    QVERIFY(w.in("sw_zero"));
    QCOMPARE(w.out, QStringList{"sw.reset"});
}

// -------------------------------------------------------------- §3.6 alarms-beep
void TstWatch::alarmsBeep()
{
    // T-60: alarm1 enabled alone
    {
        Watch w;
        w.alive();
        w.setAlarms(420, 480);
        w.click('a');
        w.click('d');                        // enable alarm1
        w.click('a');                        // alarm2 display
        w.send("T.hits.T2");                 // alarm2 disabled: nothing
        QVERIFY(w.in("alarm2"));
        w.send("T.hits.T1");                 // T-60
        FIRED(w, "T-60");
        QVERIFY(w.in("alarm1_beeps"));
        QVERIFY(!w.in("displays"));
        w.reset();
        w.click('c');                        // T-63 any button -> displays H*
        FIRED(w, "T-63");
        QVERIFY(w.in("alarm2_off"));
        QVERIFY(!w.in("update2"));           // the button is consumed by T-63
    }
    // T-61: alarm2 enabled, alarm1 enabled with a different time
    {
        Watch w;
        w.alive();
        w.setAlarms(420, 480);
        w.click('a');
        w.click('d');
        w.click('a');
        w.click('d');                        // both enabled, T1 != T2
        w.clicks("aaa");
        QVERIFY(w.in("time"));
        w.send("T.hits.T2");                 // T-61
        FIRED(w, "T-61");
        QVERIFY(w.in("alarm2_beeps"));
        w.click('b');                        // T-63
        QVERIFY(w.in("time"));
        w.send("T.hits.T1");                 // T-60 with alarm2 enabled but T1 != T2
        QVERIFY(w.in("alarm1_beeps"));
    }
    // T-62 and T-64
    {
        Watch w;
        w.alive();
        w.setAlarms(420, 420);
        w.click('a');
        w.click('d');
        w.click('a');
        w.click('d');
        w.click('a');                        // chime
        QVERIFY(w.in("chime"));
        w.reset();
        w.send("T.hits.T1");                 // T-62
        FIRED(w, "T-62");
        NOT_FIRED(w, "T-60");
        QVERIFY(w.in("both_beep"));
        w.advance(30 * kSec - 1);
        QVERIFY(w.in("both_beep"));
        w.advance(1);                        // T-64 after 30 s
        FIRED(w, "T-64");
        QVERIFY(w.in("chime"));
    }
}

// ------------------------------------------------------- §3.7 chime, light, beep-test
void TstWatch::chimeLightBeepTest()
{
    Watch w;
    w.alive();
    w.send("T.hits.hour");                   // chime disabled: nothing
    QVERIFY(w.in("chime_disabled"));
    w.clicks("aaa");
    w.click('d');                            // enable chime
    w.clicks("aa");
    QVERIFY(w.in("time") && w.in("chime_quiet"));
    w.send("T.hits.hour");                   // T-70
    FIRED(w, "T-70");
    QVERIFY(w.in("chime_beep"));
    w.advance(2 * kSec - 1);
    QVERIFY(w.in("chime_beep"));
    w.advance(1);                            // T-71 after 2 s in beep
    FIRED(w, "T-71");
    QVERIFY(w.in("chime_quiet"));

    w.down('b');                             // T-72
    FIRED(w, "T-72");
    QVERIFY(w.in("light_on"));
    QVERIFY(w.in("bt_10"));                  // T-74
    FIRED(w, "T-74");
    w.down('d');                             // T-76
    FIRED(w, "T-76");
    QVERIFY(w.in("bt_beep") && w.in("bt_beep_on"));
    QVERIFY(w.in("time"));                   // d(b up) not taken
    // In time, a leaves regular (T-10); that pre-empts the Fig. 32 refinement.
    w.reset();
    w.down('a');
    FIRED(w, "T-10");
    NOT_FIRED(w, "T-82");
    QVERIFY(w.in("alarm1") && !w.in("beep_test"));
}

void TstWatch::harelAnomalyFlagOff()
{
    // T-82 is guarded by harelAnomalies: with the flag off, a does not silence.
    Watch w(false);
    w.alive();
    w.down('d');                             // time -> date, beep-test 01
    w.down('b');                             // 01 -> beep (T-77), still in date
    QVERIFY(w.in("date") && w.in("bt_beep_on"));
    w.reset();
    w.down('a');
    NOT_FIRED(w, "T-82");
    QVERIFY(w.in("bt_beep_on") && w.in("date"));
}

// ------------------------------------- conditions, history, unexpected input

void TstWatch::alarmConditionsEqualTimes()
{
    // §3.6 with T1 == T2 and only one alarm enabled: P1/P2 hold through the
    // "other alarm disabled" term, P does not.
    {
        Watch w;                              // alarm 2 alone
        w.alive();
        w.setAlarms(420, 420);
        w.click('a');
        w.click('a');
        w.click('d');                         // alarm2 on
        w.clicks("aaa");                      // chime, stopwatch, time
        QVERIFY(w.in("time") && w.in("alarm2_enabled") && w.in("alarm1_disabled"));
        w.reset();
        w.send("T.hits.T1");                  // alarm 1 disabled: nothing
        QVERIFY(w.in("time"));
        QVERIFY(w.fired.isEmpty());
        w.send("T.hits.T2");                  // T-61 [P2]
        FIRED(w, "T-61");
        QVERIFY(w.in("alarm2_beeps"));
        QCOMPARE(w.fired, QStringList{"T-61"});
    }
    {
        Watch w;                              // alarm 1 alone
        w.alive();
        w.setAlarms(420, 420);
        w.click('a');
        w.click('d');                         // alarm1 on
        w.clicks("aaaa");                     // alarm2, chime, stopwatch, time
        QVERIFY(w.in("time") && w.in("alarm1_enabled") && w.in("alarm2_disabled"));
        w.reset();
        w.send("T.hits.T1");                  // T-60 [P1]
        FIRED(w, "T-60");
        QVERIFY(w.in("alarm1_beeps"));
        w.send("T.hits.T2");                  // already beeping: nothing more
        QVERIFY(w.in("alarm1_beeps"));
        QCOMPARE(w.fired, QStringList{"T-60"});
    }
}

void TstWatch::displaysRememberOn()
{
    // T-11/T-12 via H with the remembered value "on" (the default is "off", so only
    // "on" shows that the history was recorded).
    Watch w;
    w.alive();
    w.click('a');
    w.click('d');                             // alarm1 on
    w.click('a');
    w.click('d');                             // alarm2 on
    w.click('a');
    w.click('d');                             // chime on
    w.clicks("aa");                           // stopwatch, time
    QVERIFY(w.in("time"));
    w.click('a');
    QVERIFY(w.in("alarm1_on"));
    w.reset();
    w.click('a');
    FIRED(w, "T-11");
    QVERIFY(w.in("alarm2_on"));
    w.click('a');
    QVERIFY(w.in("chime_on"));
}

void TstWatch::historyForgottenAfterRemoval()
{
    // T-03 clh(main*) for alarm1 and alarm2: both were "on" before removal, so a
    // remembered history would bring "on" back.
    Watch w;
    w.alive();
    w.click('a');
    w.click('d');                             // alarm1 on
    w.click('a');
    w.click('d');                             // alarm2 on
    w.clicks("aaa");                          // chime, stopwatch, time
    QVERIFY(w.in("time"));
    w.send("battery.removed");                // T-03
    FIRED(w, "T-03");
    w.alive();
    w.click('a');
    QVERIFY(w.in("alarm1_off") && w.in("alarm1_disabled"));
    w.click('a');
    QVERIFY(w.in("alarm2_off") && w.in("alarm2_disabled"));
}

// T-63: each of a, b, c, d ends alarms-beep and returns to the display via H*, and
// the press does nothing else there (no toggle, no field change, no next display).
// b also switches the light, which is orthogonal (T-72/T-73).
void TstWatch::alarmEndConsumesEveryButton_data()
{
    QTest::addColumn<char>("button");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("a") << 'a' << QStringList{"T-63"};
    QTest::newRow("b") << 'b' << QStringList{"T-63", "T-72", "T-73"};
    QTest::newRow("c") << 'c' << QStringList{"T-63"};
    QTest::newRow("d") << 'd' << QStringList{"T-63"};
}

void TstWatch::alarmEndConsumesEveryButton()
{
    QFETCH(char, button);
    QFETCH(QStringList, expected);
    Watch w;
    w.alive();
    w.setAlarms(420, 480);
    w.click('a');
    w.click('d');                             // alarm1 on + enabled, alarm1 display
    w.send("T.hits.T1");
    QVERIFY(w.in("alarm1_beeps"));
    w.reset();
    w.click(button);
    FIRED(w, "T-63");
    QVERIFY(w.in("alarm1_on") && w.in("alarm1_enabled"));
    QCOMPARE(w.fired, expected);
    QVERIFY(w.out.isEmpty());                 // no increment, no stopwatch event
}

void TstWatch::unexpectedEvents()
{
    // dead ignores everything except battery.inserted
    {
        Watch w;
        for (const char *e : {"a.down", "a.up", "b.down", "b.up", "c.down", "c.up", "d.down", "d.up",
                              "T.hits.T1", "T.hits.T2", "T.hits.hour",
                              "battery.weakens", "battery.dies", "battery.removed"})
            w.send(QString::fromLatin1(e));
        QVERIFY(w.in("dead"));
        QVERIFY(w.fired.isEmpty());
        QVERIFY(w.out.isEmpty());
    }
    // battery.inserted while alive is not a fresh battery: nothing resets
    {
        Watch w;
        w.alive();
        w.click('a');
        w.click('d');                         // alarm1 on + enabled
        w.reset();
        w.send("battery.inserted");
        QVERIFY(w.in("alarm1_on") && w.in("alarm1_enabled"));
        QVERIFY(w.fired.isEmpty());
        QVERIFY(w.out.isEmpty());             // in particular no model.reset
    }
    // a weak battery does not weaken again
    {
        Watch w;
        w.alive();
        w.send("battery.weakens");
        w.reset();
        w.send("battery.weakens");
        QVERIFY(w.in("power_blink"));
        QVERIFY(w.fired.isEmpty());
    }
    // releases without presses change nothing
    {
        Watch w;
        w.alive();
        w.reset();
        for (char b : {'a', 'b', 'c', 'd'})
            w.up(b);
        QVERIFY(w.in("time") && w.in("bt_00") && w.in("light_off"));
        QVERIFY(w.fired.isEmpty());
        QVERIFY(w.out.isEmpty());
    }
}

// ================================================================== scenarios

void TstWatch::scenarioLightAndExitUpdate()
{
    // Harel p. 247: b in update turns the light on AND leaves update (orthogonality).
    Watch w;
    w.alive();
    w.enterUpdate();
    QVERIFY(w.in("update_sec"));
    w.clicks("ccc");
    QVERIFY(w.in("update_hr"));
    w.reset();
    w.down('b');
    QVERIFY(w.in("time"));
    QVERIFY(w.in("light_on"));
    FIRED(w, "T-28");
    FIRED(w, "T-72");
    w.up('b');                               // T-73
    FIRED(w, "T-73");
    QVERIFY(w.in("light_off"));
}

void TstWatch::scenarioAlarmInStopwatchLap()
{
    Watch w;
    w.alive();
    w.click('a');
    w.click('d');                            // alarm1 enabled
    w.clicks("aaa");
    w.click('b');                            // start
    w.click('d');                            // lap
    QVERIFY(w.in("sw_lap") && w.in("sw_on"));
    w.send("T.hits.T1");
    QVERIFY(w.in("alarm1_beeps"));
    w.click('a');                            // any button -> deep history
    QVERIFY(w.in("stopwatch") && w.in("sw_lap") && w.in("sw_on"));
}

void TstWatch::scenarioBothBeepOnly()
{
    Watch w;
    w.alive();
    w.setAlarms(600, 600);
    w.click('a');
    w.click('d');
    w.click('a');
    w.click('d');
    w.clicks("aaa");
    QVERIFY(w.in("time"));
    w.reset();
    w.send("T.hits.T1");
    w.send("T.hits.T2");                     // clock emits both when T1 == T2
    QVERIFY(w.in("both_beep"));
    QVERIFY(!w.in("alarm1_beeps") && !w.in("alarm2_beeps"));
    QCOMPARE(w.fired, QStringList{"T-62"});
}

void TstWatch::scenarioAlarmTimeoutReturnsToPrevious()
{
    Watch w;
    w.alive();
    w.click('a');
    w.click('d');
    w.click('a');                            // alarm2 display
    w.click('c');                            // update2.hr
    w.click('c');                            // update2.10min
    QVERIFY(w.in("update2_10min"));
    w.send("T.hits.T1");
    QVERIFY(w.in("alarm1_beeps"));
    w.advance(30 * kSec);
    QVERIFY(!w.in("alarms_beep"));
    QVERIFY(w.in("update2_10min"));          // 30 s, no button -> H*
}

void TstWatch::scenarioBatteryDiesInAlarm2Update()
{
    Watch w;
    w.alive();
    w.click('a');
    w.click('d');                            // alarm1 on
    w.click('a');
    w.click('d');                            // alarm2 on
    w.click('a');
    w.click('d');                            // chime on
    w.click('a');
    w.click('b');                            // stopwatch running
    w.click('d');                            // lap
    w.clicks("aaa");
    w.click('c');
    w.click('c');
    QVERIFY(w.in("update2_10min"));
    w.send("battery.weakens");
    w.send("battery.dies");                  // T-02 / clh(main*)
    QVERIFY(w.in("dead"));
    w.alive();
    QVERIFY(w.in("time"));
    QVERIFY(w.in("alarm1_disabled") && w.in("alarm2_disabled") && w.in("chime_disabled"));
    // history forgotten: every display comes up in its default
    w.click('a');
    QVERIFY(w.in("alarm1_off"));
    w.click('a');
    QVERIFY(w.in("alarm2_off"));
    w.click('a');
    QVERIFY(w.in("chime_off"));
    w.click('a');
    QVERIFY(w.in("sw_zero"));
    // and the same after battery.removed (T-03)
    w.click('b');
    w.clicks("aaaa");
    QVERIFY(w.in("chime_off"));
    w.click('d');
    w.send("battery.removed");
    w.alive();
    w.clicks("aaa");
    QVERIFY(w.in("chime_off") && w.in("chime_disabled"));
    w.click('a');
    QVERIFY(w.in("sw_zero"));
}

void TstWatch::scenarioBeepTestFromDate()
{
    // Fig. 31: in date, b then d. d leaves date for time (T-21 has no condition),
    // while beep-test goes 10 -> beep. Releasing d: beep -> 10 (T-80); releasing b:
    // 10 -> 00 (T-78). The display stays in time (DESIGN-NOTES D-10).
    Watch w;
    w.alive();
    w.click('d');
    QVERIFY(w.in("date"));
    w.down('b');
    QVERIFY(w.in("bt_10") && w.in("date"));
    w.down('d');
    QVERIFY(w.in("bt_beep"));
    QVERIFY(w.in("time"));
    w.reset();
    w.up('d');                               // T-80
    FIRED(w, "T-80");
    QVERIFY(w.in("bt_10"));
    w.up('b');                               // T-78
    FIRED(w, "T-78");
    QVERIFY(w.in("bt_00"));

    // the other order: d first (in time it goes to date and 01), then b
    w.down('d');
    FIRED(w, "T-20");
    FIRED(w, "T-75");
    QVERIFY(w.in("date") && w.in("bt_01"));
    w.down('b');                             // T-77
    FIRED(w, "T-77");
    QVERIFY(w.in("bt_beep") && w.in("date"));
    QVERIFY(w.in("light_on"));
    w.reset();
    w.up('b');                               // T-81
    FIRED(w, "T-81");
    QVERIFY(w.in("bt_01"));
    w.up('d');                               // T-79
    FIRED(w, "T-79");
    QVERIFY(w.in("bt_00") && w.in("date"));
    w.down('a');                             // T-83 needs a beep with a held:
    w.up('a');
    w.down('b');
    w.down('d');                             // date -> time, beep
    QVERIFY(w.in("bt_beep_on") && w.in("time"));
    w.up('d');
    w.up('b');
    // date with b+d held, a pressed and released (date: a is a no-op)
    w.down('d');                             // time -> date, 01
    w.down('b');                             // beep, still date
    QVERIFY(w.in("date") && w.in("bt_beep_on"));
    w.down('a');                             // T-82
    FIRED(w, "T-82");
    QVERIFY(w.in("bt_beep_off") && w.in("date"));
    w.up('a');                               // T-83
    FIRED(w, "T-83");
    QVERIFY(w.in("bt_beep_on"));

    // not applicable in wait, out, stopwatch
    Watch v;
    v.alive();
    v.down('c');
    v.down('b');
    v.down('d');
    QVERIFY(v.in("wait") && !v.in("beep_test"));
    v.up('d');
    v.up('b');
    v.up('c');
    v.click('a');
    v.down('b');
    v.down('d');
    QVERIFY(v.in("out") && !v.in("beep_test"));
}

void TstWatch::scenarioStopwatchKeepsRunning()
{
    Watch w;
    w.alive();
    w.clicks("aaaa");
    w.out.clear();
    w.click('b');                            // start
    QVERIFY(w.out.contains("sw.start"));
    w.out.clear();
    w.clicks("aaaaa");                       // time, alarm1, alarm2, chime, stopwatch
    QVERIFY(!w.out.contains("sw.stop"));     // leaving the display does not stop it
    QVERIFY(!w.out.contains("sw.reset"));
    QVERIFY(w.in("sw_on") && w.in("sw_reg"));
}

void TstWatch::scenarioPaperMonDThenB()
{
    // Harel p. 250: in mon, press d then b without releasing: time, month advanced
    // by one, beeper beeping, light on.
    Watch w;
    w.alive();
    w.enterUpdate();
    QVERIFY(w.in("update_sec"));
    w.clicks("cccc");
    QVERIFY(w.in("update_mon"));
    w.out.clear();
    w.down('d');
    w.down('b');
    QVERIFY(w.in("time"));
    QCOMPARE(w.out, QStringList{"model.increment:update_mon"});
    QVERIFY(w.in("bt_beep"));
    QVERIFY(w.in("light_on"));
}

QTEST_GUILESS_MAIN(TstWatch)
#include "tst_watch.moc"
