#pragma once

#include "system_monitor.h"

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QCloseEvent;
class QTableWidget;
class QDoubleSpinBox;
class QRadioButton;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *buildSidebar();
    QWidget *buildWorkspace();
    QWidget *buildHeader();
    QWidget *buildInputCard();
    QWidget *buildOutputCard();
    QWidget *buildOptionsCard();
    QWidget *buildRecommendationCard();
    QWidget *buildGraphCard();
    QWidget *buildActivityCard();

    void chooseBatchVideo();
    void chooseBatchAudio();
    void analyzeCurrentPair();
    void addBatchPair();
    void removeSelectedJobs();
    void clearBatch();
    void startBatchMux();
    void updateBatchState();
    void updateQueueTrackLabels();
    void updateCpuUsage();
    void appendLog(const QString &message);
    void loadSettings();
    void saveSettings() const;

    QTableWidget *batchTable_ = nullptr;
    QLineEdit *batchVideoPath_ = nullptr;
    QLineEdit *batchAudioPath_ = nullptr;
    QDoubleSpinBox *batchDelay_ = nullptr;
    QPushButton *addPairButton_ = nullptr;
    QLineEdit *outputPath_ = nullptr;
    QComboBox *themeCombo_ = nullptr;
    QCheckBox *defaultTrackCheck_ = nullptr;
    QCheckBox *preserveChaptersCheck_ = nullptr;
    QCheckBox *removeTagsCheck_ = nullptr;
    QCheckBox *reencodeCheck_ = nullptr;
    QComboBox *codecCombo_ = nullptr;
    QComboBox *bitrateCombo_ = nullptr;
    QRadioButton *keepAllRadio_ = nullptr;
    QRadioButton *newAudioOnlyRadio_ = nullptr;
    QRadioButton *audioFileOnlyRadio_ = nullptr;
    QLabel *methodLabel_ = nullptr;
    QLabel *offsetLabel_ = nullptr;
    QLabel *scaleLabel_ = nullptr;
    QLabel *anchorLabel_ = nullptr;
    QLabel *jobsLabel_ = nullptr;
    QLabel *analysisResultLabel_ = nullptr;
    QLabel *processingLabel_ = nullptr;
    QPlainTextEdit *logView_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QPushButton *batchButton_ = nullptr;
    QLabel *cpuUsageLabel_ = nullptr;
    QLabel *cpuTemperatureLabel_ = nullptr;
    QLabel *memoryUsageLabel_ = nullptr;
    SystemMonitor systemMonitor_;
    bool queueBusy_ = false;
};
