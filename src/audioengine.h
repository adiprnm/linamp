/* ------------------------------------------------------------------
   Linamp — native audio engine
   FFmpeg decode → EQ (10 biquad) → volume/balance → QAudioSink
   + FFT spectrum & waveform untuk visualizer.
   ------------------------------------------------------------------ */
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class AudioEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int bitrate READ bitrate NOTIFY metadataChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY metadataChanged)
    Q_PROPERTY(int channels READ channels NOTIFY metadataChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double balance READ balance WRITE setBalance NOTIFY balanceChanged)
    Q_PROPERTY(bool eqOn READ eqOn WRITE setEqOn NOTIFY eqChanged)
    Q_PROPERTY(bool eqAuto READ eqAuto WRITE setEqAuto NOTIFY eqChanged)
    Q_PROPERTY(double preamp READ preamp WRITE setPreamp NOTIFY eqChanged)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY bandsChanged)
    Q_PROPERTY(QVariantList spectrum READ spectrum NOTIFY spectrumChanged)
    Q_PROPERTY(QVariantList waveform READ waveform NOTIFY waveformChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    QString title() const;
    QString artist() const;
    QString source() const;
    qint64 duration() const;
    qint64 position() const;
    bool playing() const;
    int bitrate() const;
    int sampleRate() const;
    int channels() const;
    double volume() const;
    double balance() const;
    bool eqOn() const;
    bool eqAuto() const;
    double preamp() const;
    QVariantList bands() const;
    QVariantList spectrum() const;
    QVariantList waveform() const;
    QString lastError() const;

public slots:
    void open(const QString &url);
    void play();
    void pause();
    void togglePlay();
    void stop();
    void seek(qint64 ms);
    void setVolume(double v);
    void setBalance(double v);
    void setEqOn(bool on);
    void setEqAuto(bool on);
    void setPreamp(double db);
    void setBand(int index, double db);
    void setBands(const QVariantList &bands);
    void resetEq();
    void saveSettings();

signals:
    void metadataChanged();
    void sourceChanged();
    void durationChanged();
    void positionChanged();
    void playingChanged();
    void volumeChanged();
    void balanceChanged();
    void eqChanged();
    void bandsChanged();
    void spectrumChanged();
    void waveformChanged();
    void errorOccurred(const QString &message);
    void finished();

private:
    class Private;
    Private *d;
    Q_DISABLE_COPY(AudioEngine)
};
