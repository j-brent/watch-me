#pragma once
// The clock behind the statechart's "n sec in X" timers (Harel's timeout(entered X, n)).
//
// watch.scxml decides everything: it sends timer.start{id, secs, gen} when X is
// entered or a button restarts the countdown, and timer.cancel{id} when X is left.
// This class only counts time and, when a deadline passes, submits
// timeout.<id>{gen}. The statechart ignores a timeout whose gen is not current.
//
// Time is explicit: advance(ms) moves the clock. The app calls it from a real-time
// QTimer at 1x (setRunning); tests call it directly, so timing tests are exact and
// do not depend on how fast the machine is (DESIGN-NOTES D-20).
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariant>

class QScxmlStateMachine;

class TimeoutService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
public:
    explicit TimeoutService(QScxmlStateMachine *machine, QObject *parent = nullptr);

    // Move the clock forward; fires every deadline reached, earliest first.
    Q_INVOKABLE void advance(qint64 ms);
    qint64 now() const { return m_now; }
    // Pending deadline of a timer in ms from now, or -1 if it is not armed.
    Q_INVOKABLE qint64 remaining(const QString &id) const;

    bool running() const { return m_timer.isActive(); }
    void setRunning(bool r);    // drive the clock from real time at 1x

signals:
    void runningChanged();
    void fired(const QString &id);

private:
    struct Pending { qint64 due; QVariant gen; };
    QScxmlStateMachine *m_machine;
    QHash<QString, Pending> m_pending;
    qint64 m_now = 0;
    QTimer m_timer;
    QElapsedTimer m_real;
    qint64 m_lastReal = 0;
};
