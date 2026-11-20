#include "ButtonRouter.h"
#include "TransitionTracer.h"
#include <QScxmlStateMachine>
#include <QScxmlEvent>
#include <utility>

static bool validButton(const QString &b)
{
    return b == u"a" || b == u"b" || b == u"c" || b == u"d";
}

ButtonRouter::ButtonRouter(QScxmlStateMachine *machine, QObject *parent)
    : QObject(parent), m_machine(machine)
{
    // host.data: the statechart reports its values (DESIGN-NOTES D-24).
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent
    m_machine->connectToEvent(QStringLiteral("host.data"), this, [this](const QScxmlEvent &e) {
        const QVariantMap now = e.data().toMap();
        const QVariantMap before = std::exchange(m_data, now);
        if (before.isEmpty()) {
            emit dataValueChanged(QString());
            return;
        }
        for (auto it = now.cbegin(); it != now.cend(); ++it)
            if (before.value(it.key()) != it.value())
                emit dataValueChanged(it.key());
    });
}

QVariantMap ButtonRouter::held() const
{
    QVariantMap m;
    for (const auto &b : m_held)
        m.insert(b, true);
    return m;
}

void ButtonRouter::press(const QString &button)
{
    const QString b = button.toLower();
    if (!validButton(b) || m_held.contains(b))
        return;
    m_held.insert(b);
    emit heldChanged();
    submit(b + QLatin1String(".down"));
}

void ButtonRouter::release(const QString &button)
{
    const QString b = button.toLower();
    if (!m_held.remove(b))
        return;
    emit heldChanged();
    submit(b + QLatin1String(".up"));
}

void ButtonRouter::releaseAll()
{
    const auto held = m_held;
    for (const auto &b : held)
        release(b);
}

void ButtonRouter::submit(const QString &event)
{
    if (m_tracer)
        m_tracer->noteInput(event);
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#submitEvent-1
    m_machine->submitEvent(event);
}

void ButtonRouter::setDataValue(const QString &name, const QVariant &value)
{
    // The statechart answers with host.data, which updates dataValue().
    if (name == u"harelAnomalies")
        m_machine->submitEvent(QStringLiteral("host.anomalies"),
                               QVariantMap{{QStringLiteral("value"), value.toBool()}});
    else
        qWarning("ButtonRouter: '%s' cannot be set (DESIGN-NOTES D-24)", qPrintable(name));
}
