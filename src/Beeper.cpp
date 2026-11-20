#include "Beeper.h"

#ifdef WATCHME_HAVE_MULTIMEDIA
#include <QSoundEffect>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QUrl>
#include <cmath>

static QByteArray wav(double seconds, bool twoTone)
{
    const int rate = 22050;
    const int n = int(seconds * rate);
    QByteArray pcm;
    pcm.resize(n * 2);
    auto *s = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / rate;
        const double f = twoTone && (int(t * 8) % 2) ? 2730.0 : 2048.0;   // piezo-like tones
        s[i] = qint16(9000 * (std::sin(2 * M_PI * f * t) > 0 ? 1 : -1));
    }
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.writeRawData("RIFF", 4); ds << quint32(36 + pcm.size()); ds.writeRawData("WAVE", 4);
    ds.writeRawData("fmt ", 4); ds << quint32(16) << quint16(1) << quint16(1) << quint32(rate)
                                   << quint32(rate * 2) << quint16(2) << quint16(16);
    ds.writeRawData("data", 4); ds << quint32(pcm.size());
    ds.writeRawData(pcm.constData(), pcm.size());
    return out;
}

struct Beeper::Impl {
    QTemporaryDir dir;
    QSoundEffect shortFx, loopFx;
    Impl()
    {
        auto write = [&](const char *name, const QByteArray &data) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            if (f.open(QIODevice::WriteOnly)) f.write(data);
            return QUrl::fromLocalFile(f.fileName());
        };
        shortFx.setSource(write("short.wav", wav(0.5, false)));
        loopFx.setSource(write("loop.wav", wav(1.0, true)));
        loopFx.setLoopCount(QSoundEffect::Infinite);
    }
};
Beeper::Beeper(QObject *parent) : QObject(parent) {}  // audio objects are created on first enable
bool Beeper::available() const { return true; }
void Beeper::shortBeep() { if (m_enabled && d) d->shortFx.play(); }
void Beeper::update()
{
    if (m_enabled && !d)
        d = std::make_unique<Impl>();
    if (!d)
        return;
    if (m_enabled && m_continuous) { if (!d->loopFx.isPlaying()) d->loopFx.play(); }
    else d->loopFx.stop();
}
#else
struct Beeper::Impl {};
Beeper::Beeper(QObject *parent) : QObject(parent) {}
bool Beeper::available() const { return false; }
void Beeper::shortBeep() {}
void Beeper::update() {}
#endif

Beeper::~Beeper() = default;

void Beeper::setEnabled(bool e)
{
    if (e == m_enabled) return;
    m_enabled = e;
    update();
    emit enabledChanged();
}

void Beeper::setContinuous(bool c)
{
    if (c == m_continuous) return;
    m_continuous = c;
    update();
    emit continuousChanged();
}
