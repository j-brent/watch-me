// The LCD's content (STATECHART-SPEC §4), computed by qml/LcdView.mjs from the
// active configuration and the watch values. Expected digits come from §4: hour
// digits in 12/24-hour mode and the stopwatch wrap after 59'59"99.
#include <QtTest>
#include <QJSEngine>

class TstLcd : public QObject
{
    Q_OBJECT
    QJSEngine js;
    QJSValue run;                            // run(activeIds, values, blink) -> view

    struct W {                               // WatchModel's properties used by the view
        int hour = 12, minute = 0, second = 0, year = 1987, month = 6, day = 1, weekday = 1;
        bool is24h = false;
        int t1 = 720, t2 = 720;
        qint64 stopwatchCentis = 0, lapCentis = 0;
    };

    QJSValue view(const QStringList &active, const W &w, bool blink = false)
    {
        QJSValue v = js.newObject();
        v.setProperty("hour", w.hour); v.setProperty("minute", w.minute); v.setProperty("second", w.second);
        v.setProperty("year", w.year); v.setProperty("month", w.month); v.setProperty("day", w.day);
        v.setProperty("weekday", w.weekday); v.setProperty("is24h", w.is24h);
        v.setProperty("t1", w.t1); v.setProperty("t2", w.t2);
        v.setProperty("stopwatchCentis", double(w.stopwatchCentis));
        v.setProperty("lapCentis", double(w.lapCentis));
        const QJSValue r = run.call({js.toScriptValue(active), v, blink});
        if (r.isError())
            qWarning() << r.toString();
        return r;
    }
    QString main(const QStringList &a, const W &w) { return view(a, w).property("main").toString(); }

private slots:
    void initTestCase()
    {
        // https://doc.qt.io/qt-6/qjsengine.html#importModule
        const QJSValue mod = js.importModule(QStringLiteral(LCDVIEW_PATH));
        QVERIFY2(!mod.isError(), qPrintable(mod.toString()));
        js.globalObject().setProperty("LcdView", mod);
        run = js.evaluate("(function (ids, w, blink) {"
                          "  const s = new Set(ids);"
                          "  return LcdView.view(id => s.has(id), w, blink); })");
        QVERIFY(run.isCallable());
    }

    void hours24_data()
    {
        QTest::addColumn<int>("hour");
        QTest::addColumn<int>("minute");
        QTest::addColumn<QString>("digits");
        QTest::newRow("midnight") << 0 << 0 << "0000";
        QTest::newRow("00:59") << 0 << 59 << "0059";
        QTest::newRow("09:05") << 9 << 5 << "0905";
        QTest::newRow("10:00") << 10 << 0 << "1000";
        QTest::newRow("12:00") << 12 << 0 << "1200";
        QTest::newRow("23:59") << 23 << 59 << "2359";
    }
    void hours24()
    {
        QFETCH(int, hour);
        QFETCH(int, minute);
        QFETCH(QString, digits);
        W w; w.is24h = true; w.hour = hour; w.minute = minute;
        const QJSValue v = view({"alive", "time"}, w);
        QCOMPARE(v.property("main").toString(), digits);
        QCOMPARE(v.property("ampm").toString(), QStringLiteral("24H"));
    }

    void hours12_data()
    {
        QTest::addColumn<int>("hour");
        QTest::addColumn<int>("minute");
        QTest::addColumn<QString>("digits");
        QTest::addColumn<QString>("ampm");
        QTest::newRow("midnight") << 0 << 0 << "1200" << "AM";
        QTest::newRow("01:00") << 1 << 0 << " 100" << "AM";
        QTest::newRow("09:05") << 9 << 5 << " 905" << "AM";
        QTest::newRow("11:59") << 11 << 59 << "1159" << "AM";
        QTest::newRow("noon") << 12 << 0 << "1200" << "PM";
        QTest::newRow("13:05") << 13 << 5 << " 105" << "PM";
        QTest::newRow("23:59") << 23 << 59 << "1159" << "PM";
    }
    void hours12()
    {
        QFETCH(int, hour);
        QFETCH(int, minute);
        QFETCH(QString, digits);
        QFETCH(QString, ampm);
        W w; w.hour = hour; w.minute = minute;
        const QJSValue v = view({"alive", "time"}, w);
        QCOMPARE(v.property("main").toString(), digits);
        QCOMPARE(v.property("ampm").toString(), ampm);
    }

    void alarmDisplaysUseTheSameHourRule()
    {
        W w; w.is24h = true; w.t1 = 0; w.t2 = 7 * 60 + 15;
        QCOMPARE(main({"alive", "out", "alarm1", "alarm1_off"}, w), QStringLiteral("0000"));
        QCOMPARE(main({"alive", "out", "update2", "update2_hr"}, w), QStringLiteral("0715"));
        w.is24h = false;
        QCOMPARE(main({"alive", "out", "alarm1", "alarm1_off"}, w), QStringLiteral("1200"));
        QCOMPARE(main({"alive", "out", "alarm2", "alarm2_on"}, w), QStringLiteral(" 715"));
        QCOMPARE(view({"alive", "out", "alarm2", "alarm2_on"}, w).property("status").toString(),
                 QStringLiteral("ON"));
    }

    void stopwatchWraps_data()
    {
        QTest::addColumn<qint64>("centis");
        QTest::addColumn<QString>("digits");
        QTest::addColumn<QString>("hundredths");
        QTest::newRow("zero") << qint64(0) << "0000" << "00";
        QTest::newRow("0'00\"01") << qint64(1) << "0000" << "01";
        QTest::newRow("0'59\"99") << qint64(5999) << "0059" << "99";
        QTest::newRow("1'00\"00") << qint64(6000) << "0100" << "00";
        QTest::newRow("59'59\"99") << qint64(359999) << "5959" << "99";
        QTest::newRow("60'00\"00 wraps") << qint64(360000) << "0000" << "00";
        QTest::newRow("60'00\"01") << qint64(360001) << "0000" << "01";
        QTest::newRow("61'01\"50") << qint64(366150) << "0101" << "50";
    }
    void stopwatchWraps()
    {
        QFETCH(qint64, centis);
        QFETCH(QString, digits);
        QFETCH(QString, hundredths);
        W w; w.stopwatchCentis = centis; w.lapCentis = 123;
        QJSValue v = view({"alive", "stopwatch", "sw_nonzero", "sw_reg", "sw_on"}, w);
        QCOMPARE(v.property("main").toString(), digits);
        QCOMPARE(v.property("small").toString(), hundredths);
        QCOMPARE(v.property("lap").toBool(), false);
        // lap shows the frozen value with the same wrap
        w.stopwatchCentis = 0; w.lapCentis = centis;
        v = view({"alive", "stopwatch", "sw_nonzero", "sw_lap", "sw_on"}, w);
        QCOMPARE(v.property("main").toString(), digits);
        QCOMPARE(v.property("lap").toBool(), true);
    }

    void otherDisplays()
    {
        W w; w.month = 6; w.day = 1; w.weekday = 7;
        QJSValue v = view({"alive", "date"}, w);
        QCOMPARE(v.property("main").toString(), QStringLiteral(" 6 1"));
        QCOMPARE(v.property("label").toString(), QStringLiteral("SU"));
        QCOMPARE(main({}, w), QStringLiteral("    "));                       // dead: blank
        QCOMPARE(main({"alive", "out", "chime", "chime_on"}, w), QStringLiteral("--00"));
        // blinking field in update.hr (blink half of the cycle) vs. not blinking
        QCOMPARE(view({"alive", "update", "update_hr"}, w, true).property("blinkMain").toVariant().toList().size(), 2);
        QCOMPARE(view({"alive", "update", "update_hr"}, w, false).property("blinkMain").toVariant().toList().size(), 0);
    }
};

QTEST_GUILESS_MAIN(TstLcd)
#include "tst_lcd.moc"
