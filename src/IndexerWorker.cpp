#include "IndexerWorker.h"
#include "IndexFormat.h"

#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

IndexerWorker::IndexerWorker(QObject* parent) : QObject(parent) {}

void IndexerWorker::requestCancel() { m_cancel.store(true); }

static QString ensureRootSlash(QString root) {
    if (!root.endsWith('\\') && !root.endsWith('/')) root += "\\";
    return root;
}

void IndexerWorker::runIndex(const VolumeInfo& vol, const QString& outFile) {
    m_cancel.store(false);

    if (!vol.ok) {
        emit finished(false, "Invalid volume info.");
        return;
    }

    const QString root = ensureRootSlash(vol.root);
    emit started(QString("Indexing %1 (%2) ...").arg(root, vol.label));

    QFile f(outFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit finished(false, "Cannot open output index file for writing.");
        return;
    }

    auto writeLine = [&](const QByteArray& line) {
        f.write(line);
        f.write("\n");
    };

    writeLine("#WHEREISBYLABELQT\t1");
    writeLine("#DISK_LABEL\t" + escapeTSV(vol.label.toUtf8()));
    writeLine("#DISK_SERIAL\t" + QByteArray::number(vol.serial));
    writeLine("#DISK_FS\t" + escapeTSV(vol.fsName.toUtf8()));
    writeLine("#DISK_ROOT\t" + escapeTSV(root.toUtf8()));
    writeLine("#COLUMNS\tREL_PATH\tSIZE\tMTIME_UNIX");

    quint64 count = 0;

    QDirIterator it(root,
                    QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_cancel.load()) { emit finished(false, "Canceled."); return; }

        const QString fullPath = it.next();
        QFileInfo fi(fullPath);

        QString rel = QDir(root).relativeFilePath(fullPath);
        rel.replace('/', '\\');

        const quint64 size = (quint64)fi.size();
        const qint64 mtime = fi.lastModified().toSecsSinceEpoch();

        QByteArray line = escapeTSV(rel.toUtf8())
            + "\t" + QByteArray::number(size)
            + "\t" + QByteArray::number(mtime);

        writeLine(line);

        count++;
        if (count % 2000 == 0) emit progress(count);
    }

    emit progress(count);
    emit finished(true, QString("Done. Files indexed: %1").arg(count));
}
