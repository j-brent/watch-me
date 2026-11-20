#include "SpecTags.h"
#include <QFile>
#include <QRegularExpression>

bool SpecTags::load(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    parse(QString::fromUtf8(f.readAll()));
    return true;
}

void SpecTags::parse(const QString &markdown)
{
    static const QRegularExpression row(QStringLiteral("^\\|\\s*(T-\\d+)\\s*\\|.*\\[(P|F|A)\\]"),
                                        QRegularExpression::MultilineOption);
    auto it = row.globalMatch(markdown);
    while (it.hasNext()) {
        const auto m = it.next();
        m_tags.insert(m.captured(1), m.captured(2));
    }
}

QString SpecTags::weakest(const QStringList &specIds) const
{
    QString best;
    auto rank = [](const QString &t) { return t == u"A" ? 0 : t == u"F" ? 1 : t == u"P" ? 2 : 3; };
    for (const auto &id : specIds) {
        const QString t = tagFor(id);
        if (!t.isEmpty() && (best.isEmpty() || rank(t) < rank(best)))
            best = t;
    }
    return best;
}
