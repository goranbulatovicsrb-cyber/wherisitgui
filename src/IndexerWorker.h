#pragma once
#include <QObject>
#include <QString>
#include <atomic>

struct VolumeInfo {
    QString root;   // e.g. "E:\\"
    QString label;  // volume label
    quint32 serial = 0;
    QString fsName;
    bool ok = false;
};

class IndexerWorker : public QObject {
    Q_OBJECT
public:
    explicit IndexerWorker(QObject* parent = nullptr);
    void requestCancel();

public slots:
    void runIndex(const VolumeInfo& vol, const QString& outFile);

signals:
    void progress(quint64 filesIndexed);
    void finished(bool ok, QString message);
    void started(QString message);

private:
    std::atomic_bool m_cancel{false};
};
