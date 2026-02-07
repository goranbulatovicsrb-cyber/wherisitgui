#include "MainWindow.h"
#include "IndexFormat.h"

#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

#ifdef Q_OS_WIN
  #define NOMINMAX
  #include <windows.h>
#endif

static QString normRoot(QString r) {
    if (!r.endsWith('\\') && !r.endsWith('/')) r += "\\";
    return r;
}

#ifdef Q_OS_WIN
static VolumeInfo getVolumeInfoWin(const QString& root) {
    VolumeInfo v;
    v.root = normRoot(root);

    wchar_t volumeName[MAX_PATH] = {0};
    wchar_t fsName[MAX_PATH] = {0};
    DWORD serial = 0, maxCompLen = 0, fsFlags = 0;

    std::wstring wroot = v.root.toStdWString();

    if (GetVolumeInformationW(
            wroot.c_str(),
            volumeName, MAX_PATH,
            &serial,
            &maxCompLen,
            &fsFlags,
            fsName, MAX_PATH)) {
        v.label = QString::fromWCharArray(volumeName);
        v.serial = (quint32)serial;
        v.fsName = QString::fromWCharArray(fsName);
        v.ok = true;
    }
    return v;
}

static QList<VolumeInfo> listVolumesWin() {
    QList<VolumeInfo> out;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (mask & (1u << i)) {
            QChar letter = QChar('A' + i);
            QString root = QString("%1:\\").arg(letter);
            auto v = getVolumeInfoWin(root);
            if (v.ok) out.push_back(v);
        }
    }
    return out;
}
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WhereIsByLabelQt - Search indexed disks");

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* main = new QVBoxLayout(central);

    auto* diskBox = new QGroupBox("Disk");
    auto* diskRow = new QHBoxLayout(diskBox);
    m_volumes = new QComboBox();
    m_refreshBtn = new QPushButton("Refresh");
    diskRow->addWidget(new QLabel("Volume:"));
    diskRow->addWidget(m_volumes, 1);
    diskRow->addWidget(m_refreshBtn);
    main->addWidget(diskBox);

    auto* idxBox = new QGroupBox("Index");
    auto* idxLayout = new QVBoxLayout(idxBox);

    auto* idxRow1 = new QHBoxLayout();
    m_indexFile = new QLineEdit();
    m_browseIndexFile = new QPushButton("Browse...");
    idxRow1->addWidget(new QLabel("Index file:"));
    idxRow1->addWidget(m_indexFile, 1);
    idxRow1->addWidget(m_browseIndexFile);
    idxLayout->addLayout(idxRow1);

    auto* idxRow2 = new QHBoxLayout();
    m_indexBtn = new QPushButton("Index selected disk");
    m_cancelBtn = new QPushButton("Cancel");
    m_cancelBtn->setEnabled(false);
    idxRow2->addWidget(m_indexBtn);
    idxRow2->addWidget(m_cancelBtn);
    idxLayout->addLayout(idxRow2);

    main->addWidget(idxBox);

    auto* sBox = new QGroupBox("Search");
    auto* sLayout = new QHBoxLayout(sBox);
    m_query = new QLineEdit();
    m_onlyName = new QCheckBox("Only filename");
    m_searchBtn = new QPushButton("Search");
    sLayout->addWidget(new QLabel("Query:"));
    sLayout->addWidget(m_query, 1);
    sLayout->addWidget(m_onlyName);
    sLayout->addWidget(m_searchBtn);
    main->addWidget(sBox);

    m_results = new QTableWidget(0, 1);
    m_results->setHorizontalHeaderLabels(QStringList() << "Relative path");
    m_results->horizontalHeader()->setStretchLastSection(true);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    main->addWidget(m_results, 1);

    auto* statusRow = new QHBoxLayout();
    m_status = new QLabel("Ready.");
    m_progress = new QProgressBar();
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_progress);
    main->addLayout(statusRow);

    m_workerThread = new QThread(this);
    m_worker = new IndexerWorker();
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &IndexerWorker::started, this, &MainWindow::onIndexStarted);
    connect(m_worker, &IndexerWorker::progress, this, &MainWindow::onIndexProgress);
    connect(m_worker, &IndexerWorker::finished, this, &MainWindow::onIndexFinished);

    m_workerThread->start();

    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshVolumes);
    connect(m_browseIndexFile, &QPushButton::clicked, this, &MainWindow::chooseIndexFile);
    connect(m_indexBtn, &QPushButton::clicked, this, &MainWindow::startIndex);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::cancelIndex);
    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::runSearch);
    connect(m_query, &QLineEdit::returnPressed, this, &MainWindow::runSearch);

    connect(m_results, &QTableWidget::cellDoubleClicked, this, [this](int row, int){
        if (row < 0) return;
        const QString rel = m_results->item(row, 0)->text();
        int idx = m_volumes->currentIndex();
        if (idx >= 0 && idx < m_volumeList.size()) {
            QString full = QDir(normRoot(m_volumeList[idx].root)).filePath(rel);
            full = QDir::toNativeSeparators(full);
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(full).absolutePath()));
        }
    });

    refreshVolumes();
}

MainWindow::~MainWindow() {
    if (m_worker) m_worker->requestCancel();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(1500);
    }
}

void MainWindow::setIndexingUi(bool indexing) {
    m_isIndexing = indexing;
    m_indexBtn->setEnabled(!indexing);
    m_cancelBtn->setEnabled(indexing);
    m_refreshBtn->setEnabled(!indexing);
    m_browseIndexFile->setEnabled(!indexing);
    m_volumes->setEnabled(!indexing);

    m_progress->setVisible(indexing);
    if (!indexing) {
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
    } else {
        m_progress->setRange(0, 0);
    }
}

void MainWindow::refreshVolumes() {
#ifdef Q_OS_WIN
    m_volumeList = listVolumesWin();
    m_volumes->clear();
    for (const auto& v : m_volumeList) {
        const QString label = v.label.isEmpty() ? "(no label)" : v.label;
        m_volumes->addItem(QString("%1  [%2]  (%3)").arg(v.root, label).arg(v.serial).arg(v.fsName));
    }
    m_status->setText(QString("Found %1 volumes.").arg(m_volumeList.size()));
#else
    QMessageBox::warning(this, "Unsupported", "This build currently supports Windows volume enumeration only.");
#endif
}

void MainWindow::chooseIndexFile() {
    const QString file = QFileDialog::getSaveFileName(
        this, "Save index file", QDir::homePath(),
        "Index files (*.wibl *.wiblqt);;All files (*.*)"
    );
    if (!file.isEmpty()) m_indexFile->setText(file);
}

void MainWindow::startIndex() {
    if (m_isIndexing) return;

    int idx = m_volumes->currentIndex();
    if (idx < 0 || idx >= m_volumeList.size()) {
        QMessageBox::warning(this, "No disk", "Select a disk first.");
        return;
    }

    QString outFile = m_indexFile->text().trimmed();
    if (outFile.isEmpty()) {
        QMessageBox::warning(this, "No index file", "Choose where to save the index file.");
        return;
    }

    setIndexingUi(true);
    m_results->setRowCount(0);

    VolumeInfo vol = m_volumeList[idx];

    QMetaObject::invokeMethod(
        m_worker,
        [this, vol, outFile]() { m_worker->runIndex(vol, outFile); },
        Qt::QueuedConnection
    );
}

void MainWindow::cancelIndex() {
    if (m_worker) m_worker->requestCancel();
    m_status->setText("Cancel requested...");
}

void MainWindow::onIndexStarted(const QString& msg) { m_status->setText(msg); }

void MainWindow::onIndexProgress(quint64 files) {
    m_status->setText(QString("Indexing... %1 files").arg(files));
}

void MainWindow::onIndexFinished(bool ok, const QString& msg) {
    setIndexingUi(false);
    if (ok) {
        m_status->setText(msg);
    } else {
        m_status->setText("Failed: " + msg);
        QMessageBox::warning(this, "Index", msg);
    }
}

void MainWindow::addResultRow(const QString& relPath) {
    const int row = m_results->rowCount();
    m_results->insertRow(row);
    m_results->setItem(row, 0, new QTableWidgetItem(relPath));
}

void MainWindow::runSearch() {
    const QString indexFile = m_indexFile->text().trimmed();
    if (indexFile.isEmpty()) {
        QMessageBox::warning(this, "No index file", "Set the index file path (the one you saved during indexing).");
        return;
    }

    const QString query = m_query->text().trimmed();
    if (query.isEmpty()) {
        QMessageBox::information(this, "Search", "Type something in Query.");
        return;
    }

    QFile f(indexFile);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open index", "Cannot open index file.");
        return;
    }

    m_results->setRowCount(0);

    const QByteArray needle = query.toUtf8().toLower();
    bool onlyName = m_onlyName->isChecked();

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    quint64 hits = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;
        if (line.startsWith("#")) continue;

        QByteArray b = line.toUtf8();
        int t1 = b.indexOf('\t');
        if (t1 < 0) continue;

        QByteArray relEsc = b.left(t1);
        QByteArray relUtf8 = unescapeTSV(relEsc);

        QByteArray hay = relUtf8;
        if (onlyName) {
            int p1 = hay.lastIndexOf('\\');
            int p2 = hay.lastIndexOf('/');
            int p = (p1 > p2) ? p1 : p2;
            if (p >= 0) hay = hay.mid(p + 1);
        }

        if (hay.toLower().contains(needle)) {
            addResultRow(QString::fromUtf8(relUtf8));
            hits++;
            if (hits >= 20000) break;
        }
    }

    m_status->setText(QString("Hits: %1").arg(hits));
}
