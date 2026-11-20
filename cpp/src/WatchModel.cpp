#include "WatchModel.h"
#include <QScxmlStateMachine>
#include <QScxmlEvent>

// The simulated clock is a wall clock without time zone: use UTC as the spec so
// that addMSecs never jumps over a daylight-saving gap.
static QDateTime wall(const QDate &d, const QTime &t) { return QDateTime(d, t, Qt::UTC); }

WatchModel::WatchModel(QScxmlStateMachine *machine, QObject *parent)
    : QObject(parent), m_machine(machine)
{
    m_now = wall(QDate(1987, 6, 1), QTime(12, 0, 0));
    if (m_machine) {
        // Outgoing events of watch.scxml (DESIGN-NOTES D-11).
        // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent
        m_machine->connectToEvent(QStringLiteral("model.reset"), this, [this](const QScxmlEvent &) { reset(); });
        m_machine->connectToEvent(QStringLiteral("model.increment"), this, [this](const QScxmlEvent &e) {
            increment(e.data().toMap().value(QStringLiteral("field")).toString());
        });
        m_machine->connectToEvent(QStringLiteral("sw.start"), this, [this](const QScxmlEvent &) {
            if (!m_swRunning) { m_swRunning = true; m_swStartMs = m_simMs; emit stopwatchChanged(); }
        });
        m_machine->connectToEvent(QStringLiteral("sw.stop"), this, [this](const QScxmlEvent &) {
            if (m_swRunning) {
                m_swAccumMs += m_simMs - m_swStartMs;   // keep ms: no rounding per segment
                m_swRunning = false;
                emit stopwatchChanged();
            }
        });
        m_machine->connectToEvent(QStringLiteral("sw.lap"), this, [this](const QScxmlEvent &) {
            m_lap = stopwatchCentis();
            emit stopwatchChanged();
        });
        m_machine->connectToEvent(QStringLiteral("sw.reset"), this, [this](const QScxmlEvent &) {
            m_swAccumMs = 0; m_lap = 0; m_swRunning = false;
            emit stopwatchChanged();
        });
        m_machine->connectToEvent(QStringLiteral("beep.short"), this, [this](const QScxmlEvent &) { emit shortBeep(); });
        connect(m_machine, &QScxmlStateMachine::initializedChanged, this, &WatchModel::pushAlarmsToMachine);
    }
    m_timer.setInterval(20);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &WatchModel::onTimer);
}

int WatchModel::weekday() const
{
    return (m_now.date().dayOfWeek() - 1 + m_weekdayOffset) % 7 + 1;
}

qint64 WatchModel::stopwatchCentis() const
{
    return (m_swAccumMs + (m_swRunning ? m_simMs - m_swStartMs : 0)) / 10;
}

void WatchModel::setSpeed(double s)
{
    s = qBound(0.0, s, 3600.0);
    if (qFuzzyCompare(s, m_speed))
        return;
    m_speed = s;
    emit speedChanged();
}

void WatchModel::setRunning(bool r)
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

void WatchModel::onTimer()
{
    const qint64 nowReal = m_real.elapsed();
    const double sim = (nowReal - m_lastReal) * m_speed + m_carry;
    m_lastReal = nowReal;
    const qint64 whole = qint64(sim);
    m_carry = sim - double(whole);
    if (whole > 0)
        advance(whole);
}

void WatchModel::advance(qint64 simMs)
{
    if (simMs <= 0)
        return;
    const QDateTime before = m_now;
    m_now = m_now.addMSecs(simMs);
    m_simMs += simMs;

    // Every whole minute crossed in (before, now]: check T1/T2 and the whole hour.
    QDateTime b = wall(before.date(), QTime(before.time().hour(), before.time().minute()));
    for (QDateTime t = b.addSecs(60); t <= m_now; t = t.addSecs(60)) {
        const int mod = t.time().hour() * 60 + t.time().minute();
        if (mod == m_t1)
            submit(QStringLiteral("T.hits.T1"));
        if (mod == m_t2)
            submit(QStringLiteral("T.hits.T2"));
        if (t.time().minute() == 0)
            submit(QStringLiteral("T.hits.hour"));
    }
    // Any change of the displayed fields (s, min, h, date): whole seconds differ.
    if (before.toSecsSinceEpoch() != m_now.toSecsSinceEpoch())
        emit timeChanged();
    if (m_swRunning)
        emit stopwatchChanged();
}

void WatchModel::submit(const QString &event)
{
    emit hit(event);
    if (m_machine)
        m_machine->submitEvent(event);
}

void WatchModel::setTime(int h, int m, int s)
{
    m_now = wall(m_now.date(), QTime(qBound(0, h, 23), qBound(0, m, 59), qBound(0, s, 59)));
    emit timeChanged();
}

void WatchModel::setDate(int y, int mo, int d)
{
    QDate date(y, qBound(1, mo, 12), 1);
    date = QDate(y, date.month(), qBound(1, d, date.daysInMonth()));
    m_now = wall(date, m_now.time());
    emit timeChanged();
}

void WatchModel::setDateTime(const QDateTime &dt)
{
    m_now = wall(dt.date(), QTime(dt.time().hour(), dt.time().minute(), dt.time().second()));
    emit timeChanged();
}

void WatchModel::setAlarm(int which, int minutes)
{
    minutes = ((minutes % 1440) + 1440) % 1440;
    (which == 2 ? m_t2 : m_t1) = minutes;
    pushAlarmsToMachine();
    emit settingsChanged();
}

void WatchModel::jumpBeforeAlarm(int which, int seconds)
{
    const int target = which == 2 ? m_t2 : m_t1;
    QDateTime t = wall(m_now.date(), QTime(target / 60, target % 60));
    if (t.addSecs(-seconds) < m_now)
        t = t.addDays(1);
    setDateTime(t.addSecs(-seconds));
}

void WatchModel::pushAlarmsToMachine()
{
    // Host boundary (DESIGN-NOTES D-24): the statechart assigns T1/T2 on host.alarms.
    // Queued like every input, so a later T.hits.* sees the new values.
    if (!m_machine || !m_machine->isInitialized())
        return;
    m_machine->submitEvent(QStringLiteral("host.alarms"),
                           QVariantMap{{QStringLiteral("T1"), m_t1}, {QStringLiteral("T2"), m_t2}});
}

void WatchModel::reset()
{
    setDateTime(m_resetTime.isValid() ? m_resetTime : QDateTime::currentDateTime());
    m_weekdayOffset = 0;
    m_24h = false;
    m_t1 = m_t2 = 12 * 60;
    m_swAccumMs = 0;
    m_lap = 0;
    m_swRunning = false;
    pushAlarmsToMachine();
    emit settingsChanged();
    emit stopwatchChanged();
    emit timeChanged();
}

static int incDigit(int value, int place, int modulo)
{
    // increment one decimal digit of value (place = 1 or 10) modulo `modulo`, no carry
    const int digit = (value / place) % 10;
    return value - digit * place + ((digit + 1) % modulo) * place;
}

void WatchModel::increment(const QString &field)
{
    QTime t = m_now.time();
    QDate d = m_now.date();
    auto clampDay = [](int y, int mo, int day) {
        QDate first(y, mo, 1);
        return QDate(y, mo, qMin(day, first.daysInMonth()));
    };
    if (field == u"update_sec") {
        t = QTime(t.hour(), t.minute(), 0);                       // seconds reset to 0
    } else if (field == u"update_1min") {
        t = QTime(t.hour(), incDigit(t.minute(), 1, 10), t.second());
    } else if (field == u"update_10min") {
        t = QTime(t.hour(), incDigit(t.minute(), 10, 6), t.second());
    } else if (field == u"update_hr") {
        t = QTime((t.hour() + 1) % 24, t.minute(), t.second());
    } else if (field == u"update_mon") {
        d = clampDay(d.year(), d.month() % 12 + 1, d.day());       // date clamps to month length
    } else if (field == u"update_date") {
        d = QDate(d.year(), d.month(), d.day() % d.daysInMonth() + 1);
    } else if (field == u"update_day") {
        m_weekdayOffset = (m_weekdayOffset + 1) % 7;
    } else if (field == u"update_year") {
        d = clampDay(d.year() >= 2099 ? 1980 : d.year() + 1, d.month(), d.day());
    } else if (field == u"update_mode") {
        m_24h = !m_24h;
        emit settingsChanged();
        return;
    } else if (field.startsWith(u"update1_") || field.startsWith(u"update2_")) {
        int &a = field.startsWith(u"update1_") ? m_t1 : m_t2;
        int h = a / 60, m = a % 60;
        if (field.endsWith(u"_hr"))
            h = (h + 1) % 24;
        else if (field.endsWith(u"_10min"))
            m = incDigit(m, 10, 6);
        else if (field.endsWith(u"_1min"))
            m = incDigit(m, 1, 10);
        a = h * 60 + m;
        pushAlarmsToMachine();
        emit settingsChanged();
        return;
    } else {
        qWarning("WatchModel: unknown field '%s'", qPrintable(field));
        return;
    }
    m_now = wall(d, t);
    emit timeChanged();
}
