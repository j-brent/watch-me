#include "TimeoutService.h"
#include <QScxmlStateMachine>
#include <QScxmlEvent>
#include <QCoreApplication>
#include <limits>

TimeoutService::TimeoutService(QScxmlStateMachine *machine, QObject *parent)
    : QObject(parent), m_machine(machine)
{
    // timer.start / timer.cancel are outgoing events of watch.scxml (DESIGN-NOTES D-11).
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent
    m_machine->connectToEvent(QStringLiteral("timer.start"), this, [this](const QScxmlEvent &e) {
        const QVariantMap d = e.data().toMap();
        const qint64 ms = qRound64(d.value(QStringLiteral("secs")).toDouble() * 1000.0);
        m_pending.insert(d.value(QStringLiteral("id")).toString(), {m_now + ms, d.value(QStringLiteral("gen"))});
    });
    m_machine->connectToEvent(QStringLiteral("timer.cancel"), this, [this](const QScxmlEvent &e) {
        m_pending.remove(e.data().toMap().value(QStringLiteral("id")).toString());
    });
    m_timer.setInterval(20);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const qint64 r = m_real.elapsed();
        advance(r - m_lastReal);
        m_lastReal = r;
    });
}

// Let the machine handle what has been submitted to it, now. QScxmlStateMachine
// processes its external queue from a queued meta-call, so this delivers posted
// MetaCall events; the machine's sends (timer.start/cancel) are observed while it
// runs. https://doc.qt.io/qt-6/qcoreapplication.html#sendPostedEvents
static void drainMachine()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}

void TimeoutService::advance(qint64 ms)
{
    if (ms < 0 || m_advancing)
        return;
    m_advancing = true;
    // Timers already requested (e.g. by a button just submitted) start at the
    // current time, not at the end of this step.
    drainMachine();
    const qint64 end = m_now + ms;
    for (;;) {
        // earliest pending deadline within this step
        QString id;
        qint64 due = std::numeric_limits<qint64>::max();
        for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it)
            if (it->due <= end && (it->due < due || (it->due == due && it.key() < id))) {
                due = it->due;
                id = it.key();
            }
        if (id.isEmpty())
            break;
        m_now = due;
        const Pending p = m_pending.take(id);
        emit fired(id);
        m_machine->submitEvent(QStringLiteral("timeout.") + id,
                               QVariantMap{{QStringLiteral("gen"), p.gen}});
        // Handle the timeout at its own time `due`: a timer it starts is due
        // relative to `due` and may itself fire within this step (DESIGN-NOTES D-21).
        drainMachine();
    }
    m_now = end;
    m_advancing = false;
}

qint64 TimeoutService::remaining(const QString &id) const
{
    const auto it = m_pending.constFind(id);
    return it == m_pending.cend() ? -1 : it->due - m_now;
}

void TimeoutService::setRunning(bool r)
{
    if (r == m_timer.isActive())
        return;
    if (r) {
        m_real.start();
        m_lastReal = 0;
        m_timer.start();
    } else {
        m_timer.stop();
    }
    emit runningChanged();
}
