#pragma once
// Reads the confidence tag ([P]/[F]/[A]) of every T-xx row of
// docs/STATECHART-SPEC.md, for the badges in the statechart view.
#include <QObject>
#include <QHash>
#include <QVariantMap>

class SpecTags : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap tags READ tags CONSTANT)
public:
    explicit SpecTags(QObject *parent = nullptr) : QObject(parent) {}
    bool load(const QString &fileName);
    void parse(const QString &markdown);
    QVariantMap tags() const { return m_tags; }
    Q_INVOKABLE QString tagFor(const QString &specId) const { return m_tags.value(specId).toString(); }
    // weakest of the given rows: A < F < P ("" if none known)
    Q_INVOKABLE QString weakest(const QStringList &specIds) const;
private:
    QVariantMap m_tags;
};
