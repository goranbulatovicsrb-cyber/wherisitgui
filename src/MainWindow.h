#pragma once
#include <QMainWindow>
#include <QThread>
#include "IndexerWorker.h"

class QComboBox;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QTableWidget;
class QLabel;
class QProgressBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshVolumes();
    void chooseIndexFile();
    void startIndex();
    void cancelIndex();
    void runSearch();
    void onIndexStarted(const QString& msg);
    void onIndexProgress(quint64 files);
    void onIndexFinished(bool ok, const QString& msg);

private:
    void setIndexingUi(bool indexing);
    void addResultRow(const QString& relPath);

    QComboBox* m_volumes = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    QLineEdit* m_indexFile = nullptr;
    QPushButton* m_browseIndexFile = nullptr;

    QPushButton* m_indexBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    QLineEdit* m_query = nullptr;
    QCheckBox* m_onlyName = nullptr;
    QPushButton* m_searchBtn = nullptr;

    QTableWidget* m_results = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;

    QList<VolumeInfo> m_volumeList;

    QThread* m_workerThread = nullptr;
    IndexerWorker* m_worker = nullptr;

    bool m_isIndexing = false;
};
