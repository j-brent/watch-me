#include "ButtonRouter.h"
#include "TransitionTracer.h"
#include <QScxmlStateMachine>

static bool validButton(const QString &b)
{
    return b == u"a" || b == u"b" || b == u"c" || b == u"d";
}

ButtonRouter::ButtonRouter(QScxmlStateMachine *machine, QObject *parent)
    : QObject(parent), m_machine(machine) {}

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
