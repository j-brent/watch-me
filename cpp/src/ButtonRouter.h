#pragma once
// Turns presses/releases of the four watch buttons into SCXML events
// (a.down / a.up …). Keeps track of what is held so that keyboard auto-repeat and
// mouse+keyboard combinations never send a second "down" without an "up".
#include <QObject>
#include <QSet>
#include <QVariantMap>

class QScxmlStateMachine;
class TransitionTracer;

class ButtonRouter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap held READ held NOTIFY heldChanged)
public:
    explicit ButtonRouter(QScxmlStateMachine *machine, QObject *parent = nullptr);
    void setTracer(TransitionTracer *t) { m_tracer = t; }

    QVariantMap held() const;
    Q_INVOKABLE void press(const QString &button);   // "a".."d"
    Q_INVOKABLE void release(const QString &button);
    Q_INVOKABLE void releaseAll();
    Q_INVOKABLE void submit(const QString &event);   // any other external event
    // Sim panel: datamodel values, through the host boundary (DESIGN-NOTES D-24).
    // setDataValue sends host.<event> (only "harelAnomalies" is settable); dataValue
    // returns the last value the statechart reported in host.data.
    Q_INVOKABLE void setDataValue(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant dataValue(const QString &name) const { return m_data.value(name); }

signals:
    void heldChanged();
    // host.data changed `name`; empty: the first host.data (every value is new).
    void dataValueChanged(const QString &name);

private:
    QScxmlStateMachine *m_machine;
    TransitionTracer *m_tracer = nullptr;
    QSet<QString> m_held;
    QVariantMap m_data;     // last host.data
};
