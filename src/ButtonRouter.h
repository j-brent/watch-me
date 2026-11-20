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
    // Sim panel: write a datamodel value (e.g. the harelAnomalies flag). Write
    // through here (not the datamodel directly) so views showing it stay in sync.
    Q_INVOKABLE void setDataValue(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant dataValue(const QString &name) const;

signals:
    void heldChanged();
    // `name` was written with setDataValue; empty: the datamodel was (re)initialized.
    void dataValueChanged(const QString &name);

private:
    QScxmlStateMachine *m_machine;
    TransitionTracer *m_tracer = nullptr;
    QSet<QString> m_held;
};
