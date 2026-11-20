#pragma once
// The C++ the Rust host cannot avoid. cxx-qt (0.10) wraps neither Qt SCXML nor
// QQuickWindow, so this file holds:
//  1. WatchScxml: owns the QScxmlStateMachine loaded from watch.scxml and turns
//     what the machine does into plain Qt signals that Rust connects to
//     (connectToEvent -> machineEvent, log -> logged, reachedStableState and
//     runningChanged -> stable, initializedChanged -> initialized).
//  2. Four one-line helpers for the window (grab, resize, select:) and for
//     draining the machine's queue (docs/HOST-CONTRACT.md §5.2).
// No glue logic lives here: every decision is in rust/src/.
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

class QScxmlStateMachine;
class QQmlApplicationEngine;

class WatchScxml : public QObject
{
    Q_OBJECT
public:
    explicit WatchScxml(QObject *parent = nullptr);
    // Loads the SCXML document; returns "" or the parse errors, one per line.
    QString load(const QString &path);
    bool hasDataModel() const;
    void start();
    // Forward the machine's outgoing events matching `spec` as machineEvent.
    void observe(const QString &spec);
    void submit(const QString &event);
    void submitWithData(const QString &event, const QVariantMap &data);
    QStringList activeStateNames() const;
    bool isInitialized() const;

Q_SIGNALS:
    void machineEvent(const QString &name, const QVariantMap &data);
    void logged(const QString &label, const QString &msg);
    void stable();
    void initialized();

private:
    QScxmlStateMachine *m_machine = nullptr;
};

std::unique_ptr<WatchScxml> newWatchScxml();
// Let the machine process what has been submitted (TimeoutService::advance).
void drainMachine();
void quitApp(int code);
bool grabWindow(const QQmlApplicationEngine &engine, const QString &file);
void resizeWindow(const QQmlApplicationEngine &engine, int w, int h);
bool setChildProperty(const QQmlApplicationEngine &engine, const QString &objectName,
                      const QString &property, const QString &value);
