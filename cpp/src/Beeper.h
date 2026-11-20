#pragma once
// Optional audio for the two-tone beeper (off by default). The tone is synthesised
// into a WAV at start-up; without Qt Multimedia this class is a silent stub.
#include <QObject>
#include <memory>

class Beeper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool continuous READ continuous WRITE setContinuous NOTIFY continuousChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit Beeper(QObject *parent = nullptr);
    ~Beeper() override;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool e);
    bool continuous() const { return m_continuous; }
    void setContinuous(bool c);   // bound to "a beeping state is active"
    bool available() const;
    Q_INVOKABLE void shortBeep(); // T-56 half-second beep
signals:
    void enabledChanged();
    void continuousChanged();
private:
    void update();
    struct Impl;
    std::unique_ptr<Impl> d;
    bool m_enabled = false;
    bool m_continuous = false;
};
