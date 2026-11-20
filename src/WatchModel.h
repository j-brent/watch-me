#pragma once
// Values only (PLAN §3): the simulated clock T, the alarm times T1/T2, the
// stopwatch counter and the 12/24h flag. No modes: what is being edited, whether
// the stopwatch is shown, etc. all come from the state machine.
//
// SCXML -> C++: model.reset, model.increment{field}, sw.start/stop/lap/reset,
//               beep.short (observed with QScxmlStateMachine::connectToEvent)
// C++ -> SCXML: T.hits.T1, T.hits.T2, T.hits.hour (submitEvent), and the datamodel
//               values T1/T2 (minutes after midnight) used by conditions P1/P2/P.
#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>

class QScxmlStateMachine;

class WatchModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int hour READ hour NOTIFY timeChanged)
    Q_PROPERTY(int minute READ minute NOTIFY timeChanged)
    Q_PROPERTY(int second READ second NOTIFY timeChanged)
    Q_PROPERTY(int year READ year NOTIFY timeChanged)
    Q_PROPERTY(int month READ month NOTIFY timeChanged)
    Q_PROPERTY(int day READ day NOTIFY timeChanged)
    Q_PROPERTY(int weekday READ weekday NOTIFY timeChanged) // 1 = Monday … 7 = Sunday
    Q_PROPERTY(bool is24h READ is24h NOTIFY settingsChanged)
    Q_PROPERTY(int t1 READ t1 NOTIFY settingsChanged)        // minutes after midnight
    Q_PROPERTY(int t2 READ t2 NOTIFY settingsChanged)
    Q_PROPERTY(qint64 stopwatchCentis READ stopwatchCentis NOTIFY stopwatchChanged)
    Q_PROPERTY(qint64 lapCentis READ lapCentis NOTIFY stopwatchChanged)
    Q_PROPERTY(bool stopwatchRunning READ stopwatchRunning NOTIFY stopwatchChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
public:
    explicit WatchModel(QScxmlStateMachine *machine, QObject *parent = nullptr);

    int hour() const { return m_now.time().hour(); }
    int minute() const { return m_now.time().minute(); }
    int second() const { return m_now.time().second(); }
    int year() const { return m_now.date().year(); }
    int month() const { return m_now.date().month(); }
    int day() const { return m_now.date().day(); }
    int weekday() const;
    bool is24h() const { return m_24h; }
    int t1() const { return m_t1; }
    int t2() const { return m_t2; }
    qint64 stopwatchCentis() const;
    qint64 lapCentis() const { return m_lap; }
    bool stopwatchRunning() const { return m_swRunning; }
    double speed() const { return m_speed; }
    void setSpeed(double s);
    bool running() const { return m_timer.isActive(); }
    void setRunning(bool r);

    QDateTime now() const { return m_now; }
    Q_INVOKABLE void setTime(int h, int m, int s);
    Q_INVOKABLE void setDate(int y, int mo, int d);
    void setDateTime(const QDateTime &dt);
    Q_INVOKABLE void setAlarm(int which, int minutesAfterMidnight);
    // Sim panel: put T a few seconds before alarm 1 or 2.
    Q_INVOKABLE void jumpBeforeAlarm(int which, int seconds);

    // Advance simulated time by ms (tests and the real-time timer both use this).
    // Emits T.hits.* for every minute/hour boundary crossed.
    void advance(qint64 simMs);

    // Power-on defaults (DESIGN-NOTES D-8). If set, model.reset uses this instead of the host clock.
    void setResetTime(const QDateTime &dt) { m_resetTime = dt; }
    void reset();
    void increment(const QString &field);

signals:
    void timeChanged();
    void settingsChanged();
    void stopwatchChanged();
    void speedChanged();
    void runningChanged();
    void shortBeep();                       // T-56
    void hit(const QString &event);         // T.hits.* as submitted

private:
    void onTimer();
    void pushAlarmsToMachine();
    void submit(const QString &event);

    QScxmlStateMachine *m_machine;
    QDateTime m_now;
    QDateTime m_resetTime;
    int m_weekdayOffset = 0;   // the 'day' update field sets the weekday independently
    bool m_24h = false;
    int m_t1 = 12 * 60;
    int m_t2 = 12 * 60;
    qint64 m_swAccum = 0;      // centiseconds accumulated while stopped segments
    qint64 m_swStartMs = 0;    // simulated ms at last start
    qint64 m_simMs = 0;        // simulated ms since construction (monotonic)
    qint64 m_lap = 0;
    bool m_swRunning = false;
    double m_speed = 1.0;
    QTimer m_timer;
    QElapsedTimer m_real;
    qint64 m_lastReal = 0;
    double m_carry = 0.0;
};
