// WatchModel arithmetic and its two event boundaries with the state machine.
// Behaviour (modes) is tested in tst_watch; this file only checks values.
#include <QtTest>
#include <QSignalSpy>
#include "watch.h"
#include "WatchModel.h"

class TstModel : public QObject
{
    Q_OBJECT
private:
    static void settle() { for (int i = 0; i < 5; ++i) QCoreApplication::processEvents(); }
private slots:
    void increments();
    void dateClampAndLeapYears();
    void twelveTwentyFour();
    void alarmFields();
    void hitsEvents();
    void acceptanceAlarmWithinThreeSeconds();
    void stopwatchCountsWhileElsewhere();
    void resetOnBatteryInsert();
    void resetBeforeQueuedInput();
    void timeChangedOnWholeHours();
    void stopwatchKeepsMilliseconds();
};

void TstModel::increments()
{
    WatchModel m(nullptr);
    m.setDate(1987, 6, 1);
    m.setTime(13, 59, 42);
    m.increment("update_sec");
    QCOMPARE(m.second(), 0);                  // sec resets to 0
    m.increment("update_1min");
    QCOMPARE(m.minute(), 50);                 // 59 -> units digit wraps 9 -> 0, no carry
    m.increment("update_1min");
    QCOMPARE(m.minute(), 51);
    m.increment("update_10min");
    QCOMPARE(m.minute(), 1);                  // tens 5 -> 0
    QCOMPARE(m.hour(), 13);                   // no carry into hours
    m.setTime(23, 0, 0);
    m.increment("update_hr");
    QCOMPARE(m.hour(), 0);                    // wraps
    QCOMPARE(m.day(), 1);                     // and does not change the date
    const int wd = m.weekday();
    m.increment("update_day");
    QCOMPARE(m.weekday(), wd % 7 + 1);
    for (int i = 0; i < 6; ++i)
        m.increment("update_day");
    QCOMPARE(m.weekday(), wd);                // 7 presses: full cycle
}

void TstModel::dateClampAndLeapYears()
{
    WatchModel m(nullptr);
    m.setDate(1987, 1, 31);
    m.increment("update_mon");
    QCOMPARE(m.month(), 2);
    QCOMPARE(m.day(), 28);                    // clamped
    m.setDate(1988, 1, 31);
    m.increment("update_mon");
    QCOMPARE(m.day(), 29);                    // leap year
    m.setDate(1987, 12, 15);
    m.increment("update_mon");
    QCOMPARE(m.month(), 1);
    QCOMPARE(m.year(), 1987);                 // month wraps without carry
    m.setDate(1987, 2, 28);
    m.increment("update_date");
    QCOMPARE(m.day(), 1);                     // date wraps at month length
    QCOMPARE(m.month(), 2);
    m.setDate(1988, 2, 29);
    m.increment("update_year");
    QCOMPARE(m.year(), 1989);
    QCOMPARE(m.day(), 28);                    // Feb 29 clamps in a common year
    m.setDate(2099, 3, 1);
    m.increment("update_year");
    QCOMPARE(m.year(), 1980);
    m.setDate(2000, 2, 28);                   // 2000 is a leap year (div. by 400)
    m.setTime(23, 59, 59);
    m.advance(1000);
    QCOMPARE(m.day(), 29);
    m.setDate(1900 + 200, 2, 28);             // 2100 is not
    m.setTime(23, 59, 59);
    m.advance(1000);
    QCOMPARE(m.month(), 3);
}

void TstModel::twelveTwentyFour()
{
    WatchModel m(nullptr);
    QVERIFY(!m.is24h());
    QSignalSpy spy(&m, &WatchModel::settingsChanged);
    m.increment("update_mode");
    QVERIFY(m.is24h());
    m.increment("update_mode");
    QVERIFY(!m.is24h());
    QCOMPARE(spy.count(), 2);
}

void TstModel::alarmFields()
{
    WatchModel m(nullptr);
    m.setAlarm(1, 23 * 60 + 59);
    m.increment("update1_hr");
    QCOMPARE(m.t1(), 0 * 60 + 59);
    m.increment("update1_1min");
    QCOMPARE(m.t1(), 50);
    m.increment("update1_10min");
    QCOMPARE(m.t1(), 0);
    m.setAlarm(2, 7 * 60 + 30);
    m.increment("update2_10min");
    QCOMPARE(m.t2(), 7 * 60 + 40);
    QCOMPARE(m.t1(), 0);                      // alarm 1 untouched
}

void TstModel::hitsEvents()
{
    WatchModel m(nullptr);
    QSignalSpy spy(&m, &WatchModel::hit);
    m.setAlarm(1, 13 * 60 + 1);
    m.setAlarm(2, 13 * 60 + 1);
    m.setTime(12, 59, 58);
    m.advance(1000);
    QCOMPARE(spy.count(), 0);
    m.advance(1000);                          // 13:00:00
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("T.hits.hour"));
    m.advance(60 * 1000);                     // 13:01:00, T1 == T2: both events
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("T.hits.T1"));
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("T.hits.T2"));
    spy.clear();
    m.advance(3 * 3600 * 1000);               // large jump: every boundary crossed is reported
    QCOMPARE(spy.count(), 3);                 // 14:00, 15:00, 16:00
}

void TstModel::acceptanceAlarmWithinThreeSeconds()
{
    // PLAN P4: T set to T1 - 2 s with alarm1 enabled reaches alarm1_beeps within
    // 3 simulated seconds.
    WatchMachine sm;
    WatchModel m(&sm);
    sm.start();
    settle();
    sm.submitEvent("battery.inserted");
    settle();
    m.setAlarm(1, 7 * 60 + 15);
    sm.submitEvent("a.down"); sm.submitEvent("a.up");   // alarm1
    sm.submitEvent("d.down"); sm.submitEvent("d.up");   // enable
    sm.submitEvent("a.down"); sm.submitEvent("a.up");
    sm.submitEvent("a.down"); sm.submitEvent("a.up");
    sm.submitEvent("a.down"); sm.submitEvent("a.up");
    sm.submitEvent("a.down"); sm.submitEvent("a.up");   // back to time
    settle();
    QVERIFY(sm.isActive("alarm1_enabled") && sm.isActive("time"));
    m.jumpBeforeAlarm(1, 2);
    QCOMPARE(m.hour() * 3600 + m.minute() * 60 + m.second(), 7 * 3600 + 15 * 60 - 2);
    int simMs = 0;
    while (!sm.isActive("alarm1_beeps") && simMs < 3000) {
        m.advance(100);
        simMs += 100;
        settle();
    }
    QVERIFY(sm.isActive("alarm1_beeps"));
    QVERIFY(simMs <= 3000);
}

void TstModel::stopwatchCountsWhileElsewhere()
{
    WatchMachine sm;
    WatchModel m(&sm);
    sm.start();
    settle();
    sm.submitEvent("battery.inserted");
    for (int i = 0; i < 4; ++i) { sm.submitEvent("a.down"); sm.submitEvent("a.up"); }
    settle();
    QVERIFY(sm.isActive("sw_zero"));
    sm.submitEvent("b.down"); sm.submitEvent("b.up");      // start
    settle();
    QVERIFY(m.stopwatchRunning());
    m.advance(1500);
    QCOMPARE(m.stopwatchCentis(), 150);
    sm.submitEvent("a.down"); sm.submitEvent("a.up");      // leave to time
    settle();
    QVERIFY(sm.isActive("time"));
    m.advance(2000);
    QCOMPARE(m.stopwatchCentis(), 350);                    // still counting
    sm.submitEvent("d.down"); sm.submitEvent("d.up");      // date
    m.advance(500);
    settle();
    for (int i = 0; i < 1; ++i) { sm.submitEvent("d.down"); sm.submitEvent("d.up"); }
    for (int i = 0; i < 4; ++i) { sm.submitEvent("a.down"); sm.submitEvent("a.up"); }
    settle();
    QVERIFY(sm.isActive("sw_on"));
    QCOMPARE(m.stopwatchCentis(), 400);
    sm.submitEvent("d.down"); sm.submitEvent("d.up");      // lap freezes the shown value
    settle();
    QCOMPARE(m.lapCentis(), 400);
    m.advance(1000);
    QCOMPARE(m.lapCentis(), 400);
    QCOMPARE(m.stopwatchCentis(), 500);
    sm.submitEvent("b.down"); sm.submitEvent("b.up");      // stop
    settle();
    m.advance(1000);
    QCOMPARE(m.stopwatchCentis(), 500);
    sm.submitEvent("d.down"); sm.submitEvent("d.up");      // lap -> reg
    sm.submitEvent("d.down"); sm.submitEvent("d.up");      // reg (in off) -> zero
    settle();
    QVERIFY(sm.isActive("sw_zero"));
    QCOMPARE(m.stopwatchCentis(), 0);
}

void TstModel::resetOnBatteryInsert()
{
    WatchMachine sm;
    WatchModel m(&sm);
    const QDateTime t0(QDate(1987, 1, 2), QTime(3, 4, 5), Qt::UTC);
    m.setResetTime(t0);
    m.setAlarm(1, 5);
    sm.start();
    settle();
    sm.submitEvent("battery.inserted");
    settle();
    QCOMPARE(m.t1(), 12 * 60);
    QCOMPARE(m.hour(), 3);
    QCOMPARE(m.day(), 2);
    QCOMPARE(sm.dataModel()->scxmlProperty("T1").toInt(), 12 * 60);
}

void TstModel::resetBeforeQueuedInput()
{
    // DESIGN-NOTES D-21: model.reset (sent on entry to alive) reaches the model
    // before input queued behind battery.inserted, so it cannot undo sw.start.
    WatchMachine sm;
    WatchModel m(&sm);
    sm.start();
    settle();
    sm.submitEvent("battery.inserted");
    for (int i = 0; i < 4; ++i) { sm.submitEvent("a.down"); sm.submitEvent("a.up"); }
    sm.submitEvent("b.down");                               // T-50: start
    settle();
    QVERIFY(sm.isActive("sw_on"));
    QVERIFY(m.stopwatchRunning());
    m.advance(1000);
    QCOMPARE(m.stopwatchCentis(), 100);
}

void TstModel::timeChangedOnWholeHours()
{
    WatchModel m(nullptr);
    m.setTime(10, 20, 30);
    QSignalSpy spy(&m, &WatchModel::timeChanged);
    m.advance(3600 * 1000);                                 // only the hour changes
    QCOMPARE(m.hour(), 11);
    QCOMPARE(spy.count(), 1);
    m.advance(500);                                         // 11:20:30.5: nothing shown changes
    QCOMPARE(spy.count(), 1);
    m.advance(500);
    QCOMPARE(spy.count(), 2);
}

void TstModel::stopwatchKeepsMilliseconds()
{
    // Ten 15 ms runs are 150 ms: stopping must not round each run down to 10 ms.
    WatchMachine sm;
    WatchModel m(&sm);
    sm.start();
    settle();
    sm.submitEvent("battery.inserted");
    for (int i = 0; i < 4; ++i) { sm.submitEvent("a.down"); sm.submitEvent("a.up"); }
    settle();
    QVERIFY(sm.isActive("sw_zero"));
    for (int i = 0; i < 10; ++i) {
        sm.submitEvent("b.down"); sm.submitEvent("b.up");   // start (T-50 / T-52)
        settle();
        m.advance(15);
        sm.submitEvent("b.down"); sm.submitEvent("b.up");   // stop (T-51)
        settle();
    }
    QVERIFY(!m.stopwatchRunning());
    QCOMPARE(m.stopwatchCentis(), 15);
}

QTEST_GUILESS_MAIN(TstModel)
#include "tst_model.moc"
