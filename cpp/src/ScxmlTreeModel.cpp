#include "ScxmlTreeModel.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QCollator>
#include <algorithm>

ScxmlTreeModel::ScxmlTreeModel(QObject *parent) : QObject(parent) {}

bool ScxmlTreeModel::load(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open %1").arg(fileName);
        emit loaded();
        return false;
    }
    return loadData(f.readAll());
}

bool ScxmlTreeModel::loadData(const QByteArray &xml)
{
    m_nodes.clear();
    m_transitions.clear();
    m_order.clear();
    m_rootId.clear();
    m_error.clear();

    QXmlStreamReader r(xml);
    QStringList stack;            // open state ids
    int openTransition = -1;      // index of the <transition> being read
    int anon = 0;
    static const QRegularExpression specRe(QStringLiteral("\\bT-\\d+\\b"));

    auto addNode = [&](const QString &id, const QString &kind) {
        ScxmlNode n;
        n.id = id;
        n.kind = kind;
        if (!stack.isEmpty()) {
            n.parent = stack.last();
            m_nodes[n.parent].children << id;
        }
        n.depth = int(stack.size());
        m_nodes.insert(id, n);
        m_order << id;
    };

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const auto name = r.name();
            const auto a = r.attributes();
            if (name == u"scxml") {
                m_rootId = a.hasAttribute(QLatin1String("name")) ? a.value(QLatin1String("name")).toString() : QStringLiteral("scxml");
                addNode(m_rootId, QStringLiteral("root"));
                m_nodes[m_rootId].initial = a.value(QLatin1String("initial")).toString();
                stack << m_rootId;
            } else if (name == u"state" || name == u"parallel" || name == u"final" || name == u"history") {
                QString id = a.value(QLatin1String("id")).toString();
                if (id.isEmpty())
                    id = QStringLiteral("_anon%1").arg(++anon);
                QString kind = name == u"state" ? QStringLiteral("atomic") : name.toString();
                addNode(id, kind);
                if (name == u"history")
                    m_nodes[id].historyType = a.value(QLatin1String("type")).isEmpty() ? QStringLiteral("shallow")
                                                                         : a.value(QLatin1String("type")).toString();
                if (a.hasAttribute(QLatin1String("initial")))
                    m_nodes[id].initial = a.value(QLatin1String("initial")).toString();
                stack << id;
            } else if (name == u"transition" && !stack.isEmpty()) {
                const QString owner = stack.last();
                // the default transition of a <history> or <initial> is not a user transition
                if (m_nodes[owner].kind == u"history")
                    continue;
                // host-boundary plumbing (host.*, DESIGN-NOTES D-24) is not part of the chart
                // (only if every event of the transition is a host.* event)
                const QStringList events = a.value(QLatin1String("event")).toString().split(u' ', Qt::SkipEmptyParts);
                if (!events.isEmpty() && std::all_of(events.cbegin(), events.cend(), [](const QString &e) {
                        return e.startsWith(QLatin1String("host."));
                    }))
                    continue;
                ScxmlTransition t;
                t.source = owner;
                t.event = a.value(QLatin1String("event")).toString();
                t.cond = a.value(QLatin1String("cond")).toString();
                t.targets = a.value(QLatin1String("target")).toString().split(u' ', Qt::SkipEmptyParts);
                t.type = a.hasAttribute(QLatin1String("type")) ? a.value(QLatin1String("type")).toString() : QStringLiteral("external");
                m_transitions << t;
                openTransition = int(m_transitions.size()) - 1;
                m_nodes[owner].transitions << openTransition;
            } else if (name == u"log" && openTransition >= 0) {
                const auto m = specRe.match(a.value(QLatin1String("label")).toString());
                if (m.hasMatch() && m_transitions[openTransition].specId.isEmpty())
                    m_transitions[openTransition].specId = m.captured(0);
            }
        } else if (r.isEndElement()) {
            const auto name = r.name();
            if (name == u"state" || name == u"parallel" || name == u"final" || name == u"history"
                || name == u"scxml") {
                if (!stack.isEmpty())
                    stack.removeLast();
            } else if (name == u"transition") {
                openTransition = -1;
            }
        } else if (r.isComment() && !m_transitions.isEmpty()) {
            // Fallback: "<transition .../> <!-- T-xx -->" right after the element.
            auto &t = m_transitions.last();
            if (t.specId.isEmpty()) {
                const auto m = specRe.match(r.text().toString());
                if (m.hasMatch())
                    t.specId = m.captured(0);
            }
        }
    }
    if (r.hasError()) {
        m_error = r.errorString();
        emit loaded();
        return false;
    }

    // Classify states and resolve implicit initial children (first child state).
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        ScxmlNode &n = it.value();
        if (n.kind == u"atomic") {
            bool hasStates = std::any_of(n.children.cbegin(), n.children.cend(), [&](const QString &c) {
                const auto k = m_nodes.value(c).kind;
                return k != u"history";
            });
            if (hasStates)
                n.kind = QStringLiteral("compound");
        }
        if ((n.kind == u"compound" || n.kind == u"root") && n.initial.isEmpty()) {
            for (const auto &c : n.children)
                if (m_nodes.value(c).kind != u"history") { n.initial = c; break; }
        }
    }
    emit loaded();
    return true;
}

QVariantMap ScxmlTreeModel::transitionToVariant(const ScxmlTransition &t)
{
    return {
        {QStringLiteral("source"), t.source},
        {QStringLiteral("event"), t.event},
        {QStringLiteral("cond"), t.cond},
        {QStringLiteral("targets"), t.targets},
        {QStringLiteral("type"), t.type},
        {QStringLiteral("specId"), t.specId},
    };
}

QVariantMap ScxmlTreeModel::toVariant(const QString &id) const
{
    const ScxmlNode n = m_nodes.value(id);
    QVariantList kids;
    for (const auto &c : n.children)
        kids << toVariant(c);
    QVariantList trans;
    for (int i : n.transitions)
        trans << transitionToVariant(m_transitions[i]);
    return {
        {QStringLiteral("id"), n.id},
        {QStringLiteral("kind"), n.kind},
        {QStringLiteral("historyType"), n.historyType},
        {QStringLiteral("parent"), n.parent},
        {QStringLiteral("initial"), n.initial},
        {QStringLiteral("depth"), n.depth},
        {QStringLiteral("children"), kids},
        {QStringLiteral("transitions"), trans},
    };
}

QVariantMap ScxmlTreeModel::root() const
{
    return m_rootId.isEmpty() ? QVariantMap{} : toVariant(m_rootId);
}

QVariantMap ScxmlTreeModel::node(const QString &id) const
{
    return m_nodes.contains(id) ? toVariant(id) : QVariantMap{};
}

QVariantList ScxmlTreeModel::transitionsFrom(const QString &id) const
{
    QVariantList out;
    for (int i : m_nodes.value(id).transitions)
        out << transitionToVariant(m_transitions[i]);
    return out;
}

QVariantList ScxmlTreeModel::transitionsInto(const QString &id) const
{
    QVariantList out;
    for (const auto &t : m_transitions)
        if (t.targets.contains(id))
            out << transitionToVariant(t);
    return out;
}

QStringList ScxmlTreeModel::ancestors(const QString &id) const
{
    QStringList out;
    QString p = m_nodes.value(id).parent;
    while (!p.isEmpty()) {
        out << p;
        p = m_nodes.value(p).parent;
    }
    return out;
}

QStringList ScxmlTreeModel::specIds() const
{
    QStringList ids;
    for (const auto &t : m_transitions)
        if (!t.specId.isEmpty() && !ids.contains(t.specId))
            ids << t.specId;
    QCollator c;
    c.setNumericMode(true);
    std::sort(ids.begin(), ids.end(), c);
    return ids;
}
