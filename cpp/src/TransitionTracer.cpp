#include "TransitionTracer.h"
#include "ScxmlTreeModel.h"
#include <QScxmlStateMachine>
#include <QScxmlEvent>
#include <QFile>
#include <QCollator>
#include <QRegularExpression>
#include <algorithm>

TransitionTracer::TransitionTracer(QScxmlStateMachine *machine, ScxmlTreeModel *tree, QObject *parent)
    : QObject(parent), m_machine(machine), m_tree(tree)
{
    m_clock.start();
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#log
    connect(machine, &QScxmlStateMachine::log, this, &TransitionTracer::onLog);
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#reachedStableState
    connect(machine, &QScxmlStateMachine::reachedStableState, this, &TransitionTracer::onStable);
    connect(machine, &QScxmlStateMachine::runningChanged, this, &TransitionTracer::onStable);
    // Outgoing events (DESIGN-NOTES D-11) are logged as "◀ name {params}".
    for (const char *spec : {"model.*", "sw.*", "beep.*"}) {
        machine->connectToEvent(QString::fromLatin1(spec), this, [this](const QScxmlEvent &e) {
            QString line = QStringLiteral("◀ ") + e.name();
            const QVariantMap data = e.data().toMap();
            for (auto it = data.cbegin(); it != data.cend(); ++it)
                line += QStringLiteral(" %1=%2").arg(it.key(), it.value().toString());
            append(line);
        });
    }
    // The hv_* flags tell the two branches of a clh(main*) pair apart (onLog).
    machine->connectToEvent(QStringLiteral("host.data"), this,
                            [this](const QScxmlEvent &e) { m_data = e.data().toMap(); });
    onStable();
}

QStringList TransitionTracer::firedSpecIds() const
{
    QStringList l(m_fired.cbegin(), m_fired.cend());
    QCollator c;
    c.setNumericMode(true);
    std::sort(l.begin(), l.end(), c);
    return l;
}

void TransitionTracer::onLog(const QString &label, const QString &msg)
{
    static const QRegularExpression re(QStringLiteral("^T-\\d+$"));
    if (!re.match(label).hasMatch())
        return;
    // Qt 6.4 delivers QScxmlStateMachine::log after the macrostep (after
    // reachedStableState). So: the source must have been active in one of the last
    // few stable configurations, and the target must be active now (for a history
    // pseudostate: its parent).
    QString source;
    QStringList targets;
    if (m_tree) {
        auto wasActive = [&](const QString &id) {
            for (const auto &c : m_recent)
                if (c.contains(id))
                    return true;
            return false;
        };
        auto landed = [&](const ScxmlTransition &t) {
            for (const auto &tg : t.targets) {
                const auto n = m_tree->nodes().value(tg);
                const QString where = n.kind == u"history" ? n.parent : tg;
                if (!m_active.contains(where))
                    return false;
            }
            return true;
        };
        QList<const ScxmlTransition *> cands;
        for (const auto &t : m_tree->transitions())
            if (t.specId == label && wasActive(t.source) && landed(t))
                cands << &t;
        if (cands.isEmpty())
            for (const auto &t : m_tree->transitions())
                if (t.specId == label && wasActive(t.source))
                    cands << &t;
        const ScxmlTransition *best = cands.value(0);
        if (cands.size() > 1) {
            // Same row, same source: the clh(main*) pairs "[hv_x] -> x_H" / "-> x".
            // hv_x is only changed when x is exited, so it still tells which branch fired.
            // Its value comes from host.data (DESIGN-NOTES D-24).
            static const QRegularExpression ident(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
            const ScxmlTransition *uncond = nullptr, *taken = nullptr;
            for (const auto *t : cands) {
                if (t->cond.isEmpty()) {
                    uncond = t;
                } else if (ident.match(t->cond).hasMatch() && m_data.value(t->cond).toBool()) {
                    taken = t;
                }
            }
            best = taken ? taken : (uncond ? uncond : best);
        }
        if (best) {
            source = best->source;
            targets = best->targets;
        }
    }
    m_fired.insert(label);
    m_lastSpecId = label;
    const auto stamp = m_clock.elapsed();
    if (!source.isEmpty())
        m_flashMap.insert(source, stamp);
    for (const auto &t : targets)
        m_flashMap.insert(t, stamp);
    emit flashChanged();
    const QString to = targets.isEmpty() ? QStringLiteral("(no state change)") : targets.join(QLatin1String(", "));
    append(QStringLiteral("%1 %2: %3 → %4").arg(label, msg.isEmpty() ? QStringLiteral("·") : msg, source, to));
    emit transitionFired(label, msg, source, targets);
}

void TransitionTracer::onStable()
{
    const QStringList names = m_machine->activeStateNames(false);
    QSet<QString> now(names.cbegin(), names.cend());
    // States newly entered in this macrostep flash as well.
    for (const auto &s : now)
        if (!m_prevActive.contains(s))
            m_pendingFlash.insert(s);
    m_active = now;
    m_activeList = names;
    m_activeMap.clear();
    for (const auto &s : names)
        m_activeMap.insert(s, true);
    m_prevActive = now;
    // Only distinct configurations: a macrostep that changes no state (host.data,
    // timer.*, model.*, ...) would otherwise push the configuration a late log
    // line needs out of the window (DESIGN-NOTES D-26).
    if (m_recent.isEmpty() || m_recent.first() != now)
        m_recent.prepend(now);
    while (m_recent.size() > 4)
        m_recent.removeLast();
    emit activeChanged();
    if (!m_pendingFlash.isEmpty()) {
        m_flashMap.clear();
        const auto stamp = m_clock.elapsed();
        for (const auto &s : std::as_const(m_pendingFlash))
            m_flashMap.insert(s, stamp);
        m_pendingFlash.clear();
        emit flashChanged();
    }
}

void TransitionTracer::clearFlash()
{
    m_flashMap.clear();
    emit flashChanged();
}

void TransitionTracer::noteInput(const QString &event)
{
    append(QStringLiteral("▶ %1").arg(event));
}

void TransitionTracer::append(const QString &line)
{
    m_entries << line;
    while (m_entries.size() > maxEntries)
        m_entries.removeFirst();
    emit entriesChanged();
}

void TransitionTracer::clearLog()
{
    m_entries.clear();
    emit entriesChanged();
}

bool TransitionTracer::exportLog(const QString &fileName) const
{
    QString path = fileName;
    if (path.startsWith(QLatin1String("file://")))
        path = path.mid(7);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(logText().toUtf8());
    f.write("\n# active: ");
    f.write(m_activeList.join(QLatin1Char(' ')).toUtf8());
    f.write("\n");
    return true;
}
