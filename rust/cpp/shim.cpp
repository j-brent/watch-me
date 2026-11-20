#include "shim.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScxmlEvent>
#include <QScxmlStateMachine>

WatchScxml::WatchScxml(QObject *parent) : QObject(parent) {}

QString WatchScxml::load(const QString &path)
{
    // <log> elements are spec-row markers for the tracer, not console output.
    QLoggingCategory::setFilterRules(QStringLiteral("scxml.statemachine.debug=false"));
    // Run-time load; any parse error is fatal (docs/HOST-CONTRACT.md §2).
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#fromFile
    m_machine = QScxmlStateMachine::fromFile(path);
    if (!m_machine)
        return QStringLiteral("cannot load %1").arg(path);
    QStringList errors;
    for (const auto &e : m_machine->parseErrors())
        errors << e.toString();
    if (!errors.isEmpty())
        return errors.join(QLatin1Char('\n'));
    m_machine->setParent(this);
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#log
    connect(m_machine, &QScxmlStateMachine::log, this, &WatchScxml::logged);
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#reachedStableState
    connect(m_machine, &QScxmlStateMachine::reachedStableState, this, &WatchScxml::stable);
    connect(m_machine, &QScxmlStateMachine::runningChanged, this, &WatchScxml::stable);
    connect(m_machine, &QScxmlStateMachine::initializedChanged, this, &WatchScxml::initialized);
    return {};
}

bool WatchScxml::hasDataModel() const { return m_machine && m_machine->dataModel(); }
void WatchScxml::start() { m_machine->start(); }

void WatchScxml::observe(const QString &spec)
{
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent
    m_machine->connectToEvent(spec, this, [this](const QScxmlEvent &e) {
        emit machineEvent(e.name(), e.data().toMap());
    });
}

// https://doc.qt.io/qt-6/qscxmlstatemachine.html#submitEvent-1
void WatchScxml::submit(const QString &event) { m_machine->submitEvent(event); }
void WatchScxml::submitWithData(const QString &event, const QVariantMap &data)
{
    m_machine->submitEvent(event, data);
}

QStringList WatchScxml::activeStateNames() const { return m_machine->activeStateNames(false); }
bool WatchScxml::isInitialized() const { return m_machine->isInitialized(); }

std::unique_ptr<WatchScxml> newWatchScxml() { return std::make_unique<WatchScxml>(); }

// Qt SCXML runs its queue from a queued meta-call (DESIGN-NOTES D-21).
// https://doc.qt.io/qt-6/qcoreapplication.html#sendPostedEvents
void drainMachine() { QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall); }

void quitApp(int code) { QCoreApplication::exit(code); }

static QQuickWindow *window(const QQmlApplicationEngine &engine)
{
    return qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
}

// https://doc.qt.io/qt-6/qquickwindow.html#grabWindow
bool grabWindow(const QQmlApplicationEngine &engine, const QString &file)
{
    auto *w = window(engine);
    return w && w->grabWindow().save(file);
}

void resizeWindow(const QQmlApplicationEngine &engine, int w, int h)
{
    if (auto *win = window(engine))
        win->resize(w, h);
}

bool setChildProperty(const QQmlApplicationEngine &engine, const QString &objectName,
                      const QString &property, const QString &value)
{
    auto *root = engine.rootObjects().value(0);
    auto *o = root ? root->findChild<QObject *>(objectName) : nullptr;
    return o && o->setProperty(property.toUtf8().constData(), value);
}
