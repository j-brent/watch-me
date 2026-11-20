// ButtonRouter: the input path from the UI (mouse, keys, sim panel) to the machine.
// Its job (ButtonRouter.h): turn presses/releases of a..d into <b>.down/<b>.up and
// never send a second "down" without an "up". Invalid and repeated input is the
// point of these tests.
#include <QtTest>
#include <QSignalSpy>
#include <QScxmlEvent>
#include "watch.h"
#include "ButtonRouter.h"

class TstInput : public QObject
{
    Q_OBJECT
private:
    static void settle() { for (int i = 0; i < 5; ++i) QCoreApplication::processEvents(); }

    struct Rig {
        WatchMachine m;
        ButtonRouter r{&m};
        QStringList seen;                    // button events the machine received, in order
        Rig()
        {
            // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent ("*" = any event)
            m.connectToEvent(QStringLiteral("*"), &m, [this](const QScxmlEvent &e) {
                if (e.name().endsWith(QLatin1String(".down")) || e.name().endsWith(QLatin1String(".up")))
                    seen << e.name();
            });
            m.start();
            settle();
        }
    };

private slots:
    void initTestCase() { QLoggingCategory::setFilterRules(QStringLiteral("scxml.statemachine.debug=false")); }
    void pressAndRelease();
    void repeatedAndUnheld();
    void invalidNames();
    void releaseAll();
    void dataValues();
};

void TstInput::pressAndRelease()
{
    Rig x;
    x.r.press("A");                          // upper case is the same button
    settle();
    QCOMPARE(x.r.held().value("a").toBool(), true);
    x.r.release("a");
    settle();
    QCOMPARE(x.seen, (QStringList{"a.down", "a.up"}));
    QVERIFY(x.r.held().isEmpty());
}

void TstInput::repeatedAndUnheld()
{
    Rig x;
    QSignalSpy held(&x.r, &ButtonRouter::heldChanged);
    x.r.press("a");
    x.r.press("a");                          // keyboard auto-repeat / mouse + key
    x.r.press("A");
    x.r.release("b");                        // never pressed
    x.r.release("a");
    x.r.release("a");                        // already released
    settle();
    QCOMPARE(x.seen, (QStringList{"a.down", "a.up"}));
    QCOMPARE(held.count(), 2);               // only the real press and release
}

void TstInput::invalidNames()
{
    Rig x;
    for (const char *b : {"e", "", "ab", "1", " a"})
        x.r.press(QString::fromLatin1(b));
    settle();
    QVERIFY(x.seen.isEmpty());
    QVERIFY(x.r.held().isEmpty());
}

void TstInput::releaseAll()
{
    Rig x;
    x.r.press("b");
    x.r.press("d");
    x.r.releaseAll();
    settle();
    QCOMPARE(x.seen.size(), 4);
    QCOMPARE(x.seen.mid(0, 2), (QStringList{"b.down", "d.down"}));
    QCOMPARE(QSet<QString>(x.seen.cbegin() + 2, x.seen.cend()), (QSet<QString>{"b.up", "d.up"}));
    QVERIFY(x.r.held().isEmpty());
    x.r.releaseAll();                        // nothing held: nothing sent
    settle();
    QCOMPARE(x.seen.size(), 4);
}

void TstInput::dataValues()
{
    // The sim panel's "Fig. 32 quirk" box shows harelAnomalies through these.
    Rig x;
    // host.data at start-up (DESIGN-NOTES D-24) carries the default from watch.scxml
    QCOMPARE(x.r.dataValue("harelAnomalies").toBool(), true);
    QCOMPARE(x.r.dataValue("hv_alarm1").toBool(), false);
    QSignalSpy changed(&x.r, &ButtonRouter::dataValueChanged);
    x.r.setDataValue("harelAnomalies", false);   // host.anomalies, answered by host.data
    QCOMPARE(changed.count(), 0);                // queued: nothing until the machine runs
    settle();
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toString(), QStringLiteral("harelAnomalies"));
    QCOMPARE(x.r.dataValue("harelAnomalies").toBool(), false);
}

QTEST_GUILESS_MAIN(TstInput)
#include "tst_input.moc"
