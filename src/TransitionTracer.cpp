#include "TransitionTracer.h"
#include "ScxmlTreeModel.h"
#include <QScxmlStateMachine>
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
    // Find which transition carrying this spec id fired: the one whose source is in
    // the configuration that was stable before this macrostep (or is being entered
    // in it, for eventless/onentry chains).
    QString source;
    QStringList targets;
    if (m_tree) {
        const auto &all = m_tree->transitions();
        const ScxmlTransition *best = nullptr;
        for (const auto &t : all) {
            if (t.specId != label)
                continue;
            if (!best)
                best = &t;
            if (m_prevActive.contains(t.source)) { best = &t; break; }
        }
        if (best) {
            source = best->source;
            targets = best->targets;
        }
    }
    m_fired.insert(label);
    m_lastSpecId = label;
    if (!source.isEmpty())
        m_pendingFlash.insert(source);
    for (const auto &t : targets)
        m_pendingFlash.insert(t);
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
    f.write("\n");
    return true;
}
