#pragma once
// Observes a QScxmlStateMachine and turns microsteps into view data:
//  - the active configuration (for the highlight),
//  - the states touched by the last macrostep (for the ~600 ms flash),
//  - an event log "T-xx a.down: time → alarm1".
// Every transition in watch.scxml carries <log label="T-xx" expr="_event.name"/>,
// so QScxmlStateMachine::log tells us exactly which spec row fired. (Qt 6.4 emits
// the log signal after the macrostep; see onLog for how the source is resolved.)
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QElapsedTimer>

class QScxmlStateMachine;
class ScxmlTreeModel;

class TransitionTracer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList activeStates READ activeStates NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap activeSet READ activeSet NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap flashSet READ flashSet NOTIFY flashChanged)
    Q_PROPERTY(QStringList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QStringList firedSpecIds READ firedSpecIds NOTIFY entriesChanged)
    Q_PROPERTY(QString lastSpecId READ lastSpecId NOTIFY entriesChanged)
public:
    TransitionTracer(QScxmlStateMachine *machine, ScxmlTreeModel *tree, QObject *parent = nullptr);

    QStringList activeStates() const { return m_activeList; }
    QVariantMap activeSet() const { return m_activeMap; }
    QVariantMap flashSet() const { return m_flashMap; }
    QStringList entries() const { return m_entries; }
    QStringList firedSpecIds() const;
    QString lastSpecId() const { return m_lastSpecId; }

    Q_INVOKABLE bool isActive(const QString &id) const { return m_active.contains(id); }
    Q_INVOKABLE void noteInput(const QString &event); // external stimulus, for the log
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE QString logText() const { return m_entries.join(QLatin1Char('\n')); }
    Q_INVOKABLE bool exportLog(const QString &fileName) const;
    Q_INVOKABLE void clearFlash();

    int maxEntries = 500;

signals:
    void activeChanged();
    void flashChanged();
    void entriesChanged();
    void transitionFired(const QString &specId, const QString &event,
                         const QString &source, const QStringList &targets);

private:
    void onLog(const QString &label, const QString &msg);
    void onStable();
    void append(const QString &line);

    QScxmlStateMachine *m_machine;
    ScxmlTreeModel *m_tree;
    QSet<QString> m_active, m_prevActive, m_pendingFlash;
    QList<QSet<QString>> m_recent;   // last distinct stable configurations, newest first
    QStringList m_activeList;
    QVariantMap m_activeMap, m_flashMap;
    QStringList m_entries;
    QSet<QString> m_fired;
    QVariantMap m_data;      // last host.data: the hv_* flags (DESIGN-NOTES D-24)
    QString m_lastSpecId;
    QElapsedTimer m_clock;
};
