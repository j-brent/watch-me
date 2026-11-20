#pragma once
// Parses an SCXML document with QXmlStreamReader into a plain tree that QML can
// render recursively (StateBox.qml). Nothing here is hand-written per state: the
// live statechart view is generated from the same watch.scxml that qscxmlc compiles.
#include <QObject>
#include <QVariantMap>
#include <QHash>
#include <QStringList>

struct ScxmlTransition {
    QString source;      // id of the state the <transition> is a child of
    QString event;       // space separated descriptors, may be empty (eventless)
    QString cond;
    QStringList targets; // empty for targetless transitions
    QString type;        // "external" (default) or "internal"
    QString specId;      // T-xx from the <log label="T-xx"/> inside the transition
};

struct ScxmlNode {
    QString id;
    QString kind;        // atomic | compound | parallel | history | final | root
    QString historyType; // shallow | deep (history nodes only)
    QString parent;
    QString initial;     // default child id (compound states)
    QStringList children;
    QList<int> transitions; // indices into ScxmlTreeModel::transitions()
    int depth = 0;
};

class ScxmlTreeModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap root READ root NOTIFY loaded)
    Q_PROPERTY(QStringList stateIds READ stateIds NOTIFY loaded)
    Q_PROPERTY(QString errorString READ errorString NOTIFY loaded)
public:
    explicit ScxmlTreeModel(QObject *parent = nullptr);

    bool load(const QString &fileName);
    bool loadData(const QByteArray &xml);

    QVariantMap root() const;                 // recursive QVariantMap tree
    QStringList stateIds() const { return m_order; }
    QString errorString() const { return m_error; }

    const QHash<QString, ScxmlNode> &nodes() const { return m_nodes; }
    const QList<ScxmlTransition> &transitions() const { return m_transitions; }
    QString rootId() const { return m_rootId; }

    Q_INVOKABLE QVariantMap node(const QString &id) const;
    Q_INVOKABLE QVariantList transitionsFrom(const QString &id) const;
    Q_INVOKABLE QVariantList transitionsInto(const QString &id) const;
    Q_INVOKABLE QStringList ancestors(const QString &id) const; // nearest first
    Q_INVOKABLE QStringList specIds() const;                    // sorted, unique

signals:
    void loaded();

private:
    QVariantMap toVariant(const QString &id) const;
    static QVariantMap transitionToVariant(const ScxmlTransition &t);

    QHash<QString, ScxmlNode> m_nodes;
    QList<ScxmlTransition> m_transitions;
    QStringList m_order; // document order
    QString m_rootId;
    QString m_error;
};
