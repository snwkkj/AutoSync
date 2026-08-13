#include "main_window.h"

#include "core.h"
#include "delay_spin_box.h"
#include "icons.h"
#include "language_combo.h"

#include <QApplication>
#include <QButtonGroup>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <algorithm>
#include <utility>
#include <functional>
#include <vector>

namespace {

constexpr auto kMediaFilter =
    "Media (*.mkv *.mp4 *.m4v *.avi *.mov *.ts *.m2ts *.mts);;All files (*)";
constexpr auto kAudioFilter =
    "Audio (*.eac3 *.ec3 *.ac3 *.dts *.dtshd *.aac *.m4a *.flac *.wav *.opus *.ogg *.mka *.mp3);;"
    "Media with audio (*.mkv *.mp4 *.m4v *.ts *.m2ts *.mts);;All files (*)";
constexpr int kAnalyzedRole = Qt::UserRole + 1;
constexpr int kSourceCodecRole = Qt::UserRole + 2;
QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName != nullptr) {
        label->setObjectName(QString::fromLatin1(objectName));
    }
    return label;
}

QFrame *makeCard(const char *name = "card")
{
    auto *card = new QFrame;
    card->setObjectName(QString::fromLatin1(name));
    return card;
}

QString displayAudioCodec(const QString &path)
{
    QProcess probe;
    probe.start(QStringLiteral("ffprobe"), {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-select_streams"), QStringLiteral("a:0"),
        QStringLiteral("-show_entries"), QStringLiteral("stream=codec_name,profile"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1"), path
    });
    if (!probe.waitForStarted(1000) || !probe.waitForFinished(5000) ||
        probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        probe.kill();
        probe.waitForFinished(500);
        const QString extension = QFileInfo(path).suffix().toUpper();
        return extension.isEmpty() ? QObject::tr("Audio") : extension;
    }

    QString codec;
    QString profile;
    const QString output = QString::fromUtf8(probe.readAllStandardOutput());
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("codec_name="))) {
            codec = line.mid(11).trimmed().toLower();
        } else if (line.startsWith(QStringLiteral("profile="))) {
            profile = line.mid(8).trimmed();
        }
    }
    if (codec == QStringLiteral("eac3")) return QStringLiteral("E-AC-3");
    if (codec == QStringLiteral("ac3")) return QStringLiteral("AC-3");
    if (codec == QStringLiteral("dts")) {
        return profile.isEmpty() || profile == QStringLiteral("unknown")
                   ? QStringLiteral("DTS") : profile;
    }
    if (codec == QStringLiteral("truehd")) return QStringLiteral("TrueHD");
    if (codec == QStringLiteral("aac")) return QStringLiteral("AAC");
    if (codec == QStringLiteral("opus")) return QStringLiteral("Opus");
    if (codec == QStringLiteral("vorbis")) return QStringLiteral("Vorbis");
    if (codec == QStringLiteral("flac")) return QStringLiteral("FLAC");
    if (codec == QStringLiteral("alac")) return QStringLiteral("ALAC");
    if (codec == QStringLiteral("mp3")) return QStringLiteral("MP3");
    if (codec.startsWith(QStringLiteral("pcm_"))) return QStringLiteral("PCM");
    if (!codec.isEmpty()) return codec.toUpper();

    const QString extension = QFileInfo(path).suffix().toUpper();
    return extension.isEmpty() ? QObject::tr("Audio") : extension;
}

QString uniqueOutputPath(const QString &directory, const QString &baseName,
                         const QString &extension, QSet<QString> &reservedPaths)
{
    const QDir outputDirectory(directory);
    QString fileName = QStringLiteral("%1.%2").arg(baseName, extension);
    QString candidate = outputDirectory.filePath(fileName);
    int copyNumber = 1;

    while (QFileInfo::exists(candidate) || reservedPaths.contains(candidate)) {
        fileName = QStringLiteral("%1(%2).%3")
                       .arg(baseName)
                       .arg(copyNumber++)
                       .arg(extension);
        candidate = outputDirectory.filePath(fileName);
    }
    reservedPaths.insert(candidate);
    return candidate;
}

QWidget *makeMetric(const QString &title, QLabel **valueLabel, const QString &value)
{
    auto *card = makeCard("metricCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(3);
    layout->addWidget(makeLabel(title, "mutedLabel"));
    *valueLabel = makeLabel(value);
    (*valueLabel)->setStyleSheet(QStringLiteral("font-weight: 800; color: #edf4fb;"));
    layout->addWidget(*valueLabel);
    return card;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("AutoSync"));
    setMinimumSize(1080, 680);
    resize(1400, 820);

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("root"));
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());
    layout->addWidget(buildWorkspace(), 1);
    setCentralWidget(root);
    qApp->installEventFilter(this);

    loadSettings();
    appendLog(tr("C engine v%1 loaded. Interface ready.")
                  .arg(QString::fromUtf8(autosync_core_version())));

    auto *cpuTimer = new QTimer(this);
    connect(cpuTimer, &QTimer::timeout, this, &MainWindow::updateCpuUsage);
    updateCpuUsage();
    cpuTimer->start(1000);
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(72);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 12, 8, 12);
    layout->setSpacing(5);

    auto *mark = makeLabel(QStringLiteral("AS"), "appMark");
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(40, 40);
    mark->setToolTip(tr("AutoSync 0.1.0"));
    layout->addWidget(mark, 0, Qt::AlignHCenter);
    layout->addSpacing(12);

    const std::array<std::pair<QIcon, QString>, 4> navItems = {
        std::pair{autosync::icons::synchronize(), tr("Audio delay and multiplexing")},
        std::pair{autosync::icons::unavailable(), tr("Not implemented yet")},
        std::pair{autosync::icons::unavailable(), tr("Not implemented yet")},
        std::pair{autosync::icons::unavailable(), tr("Not implemented yet")}
    };
    auto *navigation = new QButtonGroup(sidebar);
    navigation->setExclusive(true);
    for (qsizetype index = 0; index < static_cast<qsizetype>(navItems.size()); ++index) {
        const auto &item = navItems.at(static_cast<std::size_t>(index));
        auto *button = new QPushButton;
        button->setIcon(item.first);
        button->setIconSize(QSize(20, 20));
        button->setToolTip(item.second);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setChecked(index == 0);
        button->setEnabled(index == 0);
        navigation->addButton(button);
        layout->addWidget(button);
    }
    layout->addStretch();

    themeCombo_ = new QComboBox(sidebar);
    themeCombo_->addItems({QStringLiteral("Professional Dark"),
                           QStringLiteral("High Contrast")});
    themeCombo_->hide();

    auto *aboutButton = new QPushButton;
    aboutButton->setObjectName(QStringLiteral("sideAction"));
    aboutButton->setIcon(autosync::icons::about());
    aboutButton->setIconSize(QSize(19, 19));
    aboutButton->setToolTip(tr("About AutoSync"));
    auto *exitButton = new QPushButton(QStringLiteral("⏻"));
    exitButton->setObjectName(QStringLiteral("sideAction"));
    exitButton->setToolTip(tr("Exit"));
    connect(aboutButton, &QPushButton::clicked, this, [this] {
        QMessageBox::about(this, tr("About AutoSync"),
                           tr("Qt 6/C++20 interface with a native C17 engine."));
    });
    auto *logsButton = new QPushButton;
    logsButton->setObjectName(QStringLiteral("sideAction"));
    logsButton->setIcon(autosync::icons::bug());
    logsButton->setIconSize(QSize(19, 19));
    logsButton->setToolTip(tr("Logs"));
    connect(logsButton, &QPushButton::clicked, this, [this] {
        if (logView_ != nullptr) {
            logView_->setFocus();
        }
    });
    connect(exitButton, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(logsButton, 0, Qt::AlignHCenter);
    layout->addWidget(aboutButton, 0, Qt::AlignHCenter);
    layout->addWidget(exitButton, 0, Qt::AlignHCenter);

    return sidebar;
}

QWidget *MainWindow::buildWorkspace()
{
    auto *container = new QWidget;
    container->setObjectName(QStringLiteral("workspace"));
    auto *outer = new QVBoxLayout(container);
    outer->setContentsMargins(18, 0, 18, 16);
    outer->setSpacing(12);
    outer->addWidget(buildHeader());

    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);

    auto *left = new QVBoxLayout;
    left->setSpacing(14);
    left->addWidget(buildInputCard(), 1);
    left->addWidget(buildOutputCard());
    columns->addLayout(left, 5);

    auto *right = new QVBoxLayout;
    right->setSpacing(14);
    right->addWidget(buildRecommendationCard());
    right->addWidget(buildOptionsCard(), 1);
    right->addWidget(buildActivityCard());
    columns->addLayout(right, 3);

    outer->addLayout(columns, 1);
    return container;
}

QWidget *MainWindow::buildHeader()
{
    auto *card = makeCard("headerCard");
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(2, 12, 2, 10);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(1);
    titles->addWidget(makeLabel(QStringLiteral("AUTOSYNC / AUDIO PROCESSING"), "eyebrow"));
    titles->addWidget(makeLabel(QStringLiteral("Analyze, delay and multiplex audio"), "pageTitle"));
    layout->addLayout(titles);
    layout->addStretch();

    auto *cpuIndicator = new QFrame;
    cpuIndicator->setObjectName(QStringLiteral("cpuIndicator"));
    cpuIndicator->setFixedSize(76, 36);
    cpuIndicator->setToolTip(tr("CPU usage"));
    auto *cpuLayout = new QHBoxLayout(cpuIndicator);
    cpuLayout->setContentsMargins(9, 0, 9, 0);
    cpuLayout->setSpacing(7);
    auto *cpuIcon = new QLabel;
    cpuIcon->setPixmap(autosync::icons::cpu());
    cpuIcon->setFixedSize(18, 18);
    cpuLayout->addWidget(cpuIcon);
    cpuUsageLabel_ = new QLabel(QStringLiteral("--%"));
    cpuUsageLabel_->setAlignment(Qt::AlignCenter);
    cpuUsageLabel_->setFixedWidth(31);
    cpuLayout->addWidget(cpuUsageLabel_);
    layout->addWidget(cpuIndicator);

    auto *temperatureIndicator = new QFrame;
    temperatureIndicator->setObjectName(QStringLiteral("temperatureIndicator"));
    temperatureIndicator->setFixedSize(76, 36);
    temperatureIndicator->setToolTip(tr("CPU temperature"));
    auto *temperatureLayout = new QHBoxLayout(temperatureIndicator);
    temperatureLayout->setContentsMargins(9, 0, 9, 0);
    temperatureLayout->setSpacing(6);
    auto *temperatureIcon = new QLabel;
    temperatureIcon->setPixmap(autosync::icons::temperature());
    temperatureIcon->setFixedSize(18, 18);
    temperatureLayout->addWidget(temperatureIcon);
    cpuTemperatureLabel_ = new QLabel(QStringLiteral("--°C"));
    cpuTemperatureLabel_->setAlignment(Qt::AlignCenter);
    cpuTemperatureLabel_->setFixedWidth(33);
    temperatureLayout->addWidget(cpuTemperatureLabel_);

    auto *memoryIndicator = new QFrame;
    memoryIndicator->setObjectName(QStringLiteral("memoryIndicator"));
    memoryIndicator->setFixedSize(76, 36);
    memoryIndicator->setToolTip(tr("RAM usage"));
    auto *memoryLayout = new QHBoxLayout(memoryIndicator);
    memoryLayout->setContentsMargins(9, 0, 9, 0);
    memoryLayout->setSpacing(7);
    auto *memoryIcon = new QLabel;
    memoryIcon->setPixmap(autosync::icons::memory());
    memoryIcon->setFixedSize(18, 18);
    memoryLayout->addWidget(memoryIcon);
    memoryUsageLabel_ = new QLabel(QStringLiteral("--%"));
    memoryUsageLabel_->setAlignment(Qt::AlignCenter);
    memoryUsageLabel_->setFixedWidth(31);
    memoryLayout->addWidget(memoryUsageLabel_);
    layout->addWidget(memoryIndicator);
    layout->addWidget(temperatureIndicator);
    return card;
}

QWidget *MainWindow::buildInputCard()
{
    auto *card = makeCard();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(10);

    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("MULTIPLEXING QUEUE"), "sectionTitle"));
    heading->addStretch();
    jobsLabel_ = makeLabel(tr("0 jobs"), "mutedLabel");
    heading->addWidget(jobsLabel_);
    layout->addLayout(heading);

    auto *notice = new QLabel(tr(
        "CONSTANT-DELAY MODE — with Re-encode audio disabled, AutoSync copies the original "
        "track bit for bit. Enable encoding only when codec conversion is required."));
    notice->setWordWrap(true);
    notice->setStyleSheet(QStringLiteral(
        "background:#20282d; border-left:3px solid #74c7ec; color:#c6d7e0; "
        "padding:9px 11px;"));
    layout->addWidget(notice);

    auto *pairEditor = new QGridLayout;
    pairEditor->setHorizontalSpacing(10);
    pairEditor->setVerticalSpacing(8);

    pairEditor->addWidget(new QLabel(tr("Target video")), 0, 0);
    batchVideoPath_ = new QLineEdit;
    batchVideoPath_->setPlaceholderText(tr("Select the MKV that will receive the audio"));
    batchVideoPath_->setClearButtonEnabled(true);
    pairEditor->addWidget(batchVideoPath_, 0, 1);
    auto *selectVideo = new QPushButton(tr("Select video"));
    connect(selectVideo, &QPushButton::clicked, this, &MainWindow::chooseBatchVideo);
    pairEditor->addWidget(selectVideo, 0, 2);

    pairEditor->addWidget(new QLabel(tr("Audio file")), 1, 0);
    batchAudioPath_ = new QLineEdit;
    batchAudioPath_->setPlaceholderText(tr("E-AC-3, AC-3, DTS, AAC, FLAC, MKA…"));
    batchAudioPath_->setClearButtonEnabled(true);
    pairEditor->addWidget(batchAudioPath_, 1, 1);
    auto *selectAudio = new QPushButton(tr("Select audio"));
    connect(selectAudio, &QPushButton::clicked, this, &MainWindow::chooseBatchAudio);
    pairEditor->addWidget(selectAudio, 1, 2);

    pairEditor->addWidget(new QLabel(tr("Delay")), 2, 0);
    batchDelay_ = new DelaySpinBox;
    batchDelay_->setRange(-86400000, 86400000);
    batchDelay_->setDecimals(3);
    batchDelay_->setSingleStep(1.0);
    batchDelay_->setValue(0);
    batchDelay_->setSuffix(QStringLiteral(" ms"));
    batchDelay_->setToolTip(tr("A positive value delays the audio; a negative value advances it."));
    pairEditor->addWidget(batchDelay_, 2, 1);
    addPairButton_ = new QPushButton(tr("Add to queue"));
    addPairButton_->setObjectName(QStringLiteral("primaryButton"));
    addPairButton_->setEnabled(false);
    connect(addPairButton_, &QPushButton::clicked, this, &MainWindow::addBatchPair);
    pairEditor->addWidget(addPairButton_, 2, 2);
    auto *editorFooter = new QWidget;
    auto *editorFooterLayout = new QHBoxLayout(editorFooter);
    editorFooterLayout->setContentsMargins(0, 0, 0, 0);
    editorFooterLayout->setSpacing(8);
    analysisResultLabel_ = makeLabel(
        tr("Select files to measure their offset."), "mutedLabel");
    analysisResultLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    editorFooterLayout->addWidget(analysisResultLabel_, 1);
    pairEditor->addWidget(editorFooter, 3, 0, 1, 3);
    pairEditor->setColumnStretch(1, 1);
    layout->addLayout(pairEditor);

    auto updateAddButton = [this] {
        const bool ready = !batchVideoPath_->text().trimmed().isEmpty() &&
                           !batchAudioPath_->text().trimmed().isEmpty();
        addPairButton_->setEnabled(ready);
    };
    connect(batchVideoPath_, &QLineEdit::textChanged, this, updateAddButton);
    connect(batchAudioPath_, &QLineEdit::textChanged, this, updateAddButton);

    batchTable_ = new QTableWidget(0, 7);
    batchTable_->setHorizontalHeaderLabels({QString(), tr("Target video"), tr("Audio source"),
                                             tr("Track"), tr("Language"),
                                             tr("Delay"), tr("Status")});
    batchTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    batchTable_->setColumnWidth(0, 38);
    batchTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    batchTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    batchTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    batchTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    batchTable_->setColumnWidth(4, 86);
    batchTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    batchTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    batchTable_->setColumnWidth(6, 120);
    batchTable_->verticalHeader()->setVisible(false);
    batchTable_->verticalHeader()->setMinimumSectionSize(36);
    batchTable_->verticalHeader()->setDefaultSectionSize(36);
    batchTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    batchTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    batchTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    batchTable_->setAlternatingRowColors(true);
    batchTable_->setMinimumHeight(85);
    batchTable_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(batchTable_);

    auto *actions = new QHBoxLayout;
    auto *remove = new QPushButton(tr("Remove checked"));
    auto *clear = new QPushButton(tr("Clear queue"));
    connect(remove, &QPushButton::clicked, this, &MainWindow::removeSelectedJobs);
    connect(clear, &QPushButton::clicked, this, &MainWindow::clearBatch);
    actions->addWidget(remove);
    actions->addWidget(clear);
    actions->addStretch();
    layout->addLayout(actions);
    return card;
}

QWidget *MainWindow::buildOutputCard()
{
    auto *card = makeCard();
    auto *layout = new QGridLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("OUTPUT"), "sectionTitle"),
                      0, 0, 1, 3);
    layout->addWidget(new QLabel(tr("Output folder")), 1, 0);
    outputPath_ = new QLineEdit;
    outputPath_->setPlaceholderText(tr("Use each video's original folder"));
    layout->addWidget(outputPath_, 1, 1);
    auto *selectOutput = new QPushButton(tr("Browse"));
    connect(selectOutput, &QPushButton::clicked, this, [this] {
        const QString selected = QFileDialog::getExistingDirectory(
            this, tr("Select output folder"), outputPath_->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
        if (!selected.isEmpty()) {
            outputPath_->setText(selected);
        }
    });
    layout->addWidget(selectOutput, 1, 2);
    layout->setColumnStretch(1, 1);
    return card;
}

QWidget *MainWindow::buildOptionsCard()
{
    auto *card = makeCard();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto *sections = new QHBoxLayout;
    sections->setSpacing(16);
    auto *mux = new QVBoxLayout;
    mux->addWidget(makeLabel(QStringLiteral("OUTPUT CONTENT"), "sectionTitle"));
    keepAllRadio_ = new QRadioButton(tr("Keep all video tracks"));
    keepAllRadio_->setChecked(true);
    mux->addWidget(keepAllRadio_);
    newAudioOnlyRadio_ = new QRadioButton(tr("Video + added audio only"));
    mux->addWidget(newAudioOnlyRadio_);
    audioFileOnlyRadio_ = new QRadioButton(tr("Generate audio only (.mka)"));
    audioFileOnlyRadio_->setToolTip(
        tr("Creates an MKA without video or re-encoding. A negative delay only removes "
           "packets that would occur before 00:00."));
    mux->addWidget(audioFileOnlyRadio_);
    auto *audioOnlyHint = makeLabel(
        tr("MKA: negative delay discards packets that would occur before 00:00."),
        "mutedLabel");
    audioOnlyHint->setWordWrap(true);
    mux->addWidget(audioOnlyHint);
    mux->addStretch();
    sections->addLayout(mux, 1);

    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setStyleSheet(QStringLiteral("color:#3a3a3a;"));
    sections->addWidget(separator);

    auto *advanced = new QVBoxLayout;
    advanced->addWidget(makeLabel(QStringLiteral("NEW TRACK PROPERTIES"), "sectionTitle"));
    defaultTrackCheck_ = new QCheckBox(tr("Set added audio as default"));
    defaultTrackCheck_->setChecked(true);
    advanced->addWidget(defaultTrackCheck_);
    preserveChaptersCheck_ = new QCheckBox(tr("Preserve chapters"));
    preserveChaptersCheck_->setChecked(true);
    preserveChaptersCheck_->setToolTip(
        tr("Keep the chapter markers from the target video."));
    advanced->addWidget(preserveChaptersCheck_);
    removeTagsCheck_ = new QCheckBox(tr("Remove container tags"));
    removeTagsCheck_->setChecked(true);
    removeTagsCheck_->setToolTip(
        tr("Exclude global tags and track tags from all input files."));
    advanced->addWidget(removeTagsCheck_);
    advanced->addStretch();
    sections->addLayout(advanced, 1);
    layout->addLayout(sections);

    batchButton_ = new QPushButton(tr("ANALYZE FILES"));
    batchButton_->setObjectName(QStringLiteral("primaryButton"));
    batchButton_->setEnabled(false);
    connect(batchButton_, &QPushButton::clicked, this, [this] {
        bool allAnalyzed = batchTable_->rowCount() > 0;
        for (int row = 0; row < batchTable_->rowCount(); ++row) {
            allAnalyzed = allAnalyzed &&
                          batchTable_->item(row, 6)->data(kAnalyzedRole).toBool();
        }
        if (allAnalyzed) {
            startBatchMux();
        } else {
            analyzeCurrentPair();
        }
    });
    layout->addWidget(batchButton_);
    return card;
}

QWidget *MainWindow::buildRecommendationCard()
{
    auto *card = makeCard();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("PROCESSING MODE"), "mutedLabel"));
    methodLabel_ = makeLabel(QStringLiteral("AUDIO DELAY SYNC & MUX"), "recommendation");
    methodLabel_->setWordWrap(true);
    layout->addWidget(methodLabel_);
    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(8);
    metrics->addWidget(makeMetric(tr("Re-encoding"), &offsetLabel_, QStringLiteral("None")));
    metrics->addWidget(makeMetric(tr("Audio"), &scaleLabel_, QStringLiteral("Bit-perfect")));
    metrics->addWidget(makeMetric(tr("Queued"), &anchorLabel_, QStringLiteral("0")));
    layout->addLayout(metrics);

    reencodeCheck_ = new QCheckBox(tr("Re-encode audio"));
    reencodeCheck_->setToolTip(
        tr("Disabled by default. When enabled, FFmpeg converts the audio before multiplexing."));
    layout->addWidget(reencodeCheck_);

    auto *encodingOptions = new QHBoxLayout;
    codecCombo_ = new QComboBox;
    codecCombo_->addItem(QStringLiteral("E-AC-3"), QStringLiteral("eac3"));
    codecCombo_->addItem(QStringLiteral("AC-3"), QStringLiteral("ac3"));
    codecCombo_->addItem(QStringLiteral("AAC"), QStringLiteral("aac"));
    codecCombo_->addItem(QStringLiteral("Opus"), QStringLiteral("libopus"));
    codecCombo_->addItem(QStringLiteral("FLAC"), QStringLiteral("flac"));
    codecCombo_->setEnabled(false);
    encodingOptions->addWidget(codecCombo_, 1);

    bitrateCombo_ = new QComboBox;
    for (const int bitrate : {192, 256, 384, 448, 640, 768, 1024}) {
        bitrateCombo_->addItem(tr("%1 kb/s").arg(bitrate), bitrate);
    }
    bitrateCombo_->setCurrentIndex(4);
    bitrateCombo_->setEnabled(false);
    encodingOptions->addWidget(bitrateCombo_, 1);
    layout->addLayout(encodingOptions);

    connect(reencodeCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        codecCombo_->setEnabled(enabled);
        bitrateCombo_->setEnabled(enabled &&
                                  codecCombo_->currentData().toString() != QStringLiteral("flac"));
        offsetLabel_->setText(enabled ? tr("Enabled") : tr("None"));
        scaleLabel_->setText(enabled ? codecCombo_->currentText() : tr("Bit-perfect"));
        updateQueueTrackLabels();
    });
    connect(codecCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const bool lossless = codecCombo_->currentData().toString() == QStringLiteral("flac");
        bitrateCombo_->setEnabled(reencodeCheck_->isChecked() && !lossless);
        if (reencodeCheck_->isChecked()) {
            scaleLabel_->setText(codecCombo_->currentText());
        }
        updateQueueTrackLabels();
    });
    return card;
}

QWidget *MainWindow::buildGraphCard()
{
    auto *card = makeCard();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->addWidget(makeLabel(QStringLiteral("HOW THIS MODE WORKS"), "sectionTitle"));

    auto *placeholder = new QFrame;
    placeholder->setObjectName(QStringLiteral("graphPlaceholder"));
    placeholder->setMinimumHeight(190);
    placeholder->setStyleSheet(QStringLiteral(
        "QFrame#graphPlaceholder { background: #1d1d1d; border: 1px solid #343434; "
        "border-radius: 3px; }"));
    auto *placeholderLayout = new QVBoxLayout(placeholder);
    auto *line = makeLabel(QStringLiteral("VIDEO  +  ORIGINAL AUDIO  +  DELAY  →  MKV"));
    line->setAlignment(Qt::AlignCenter);
    line->setStyleSheet(QStringLiteral("font-size: 22px; color: #477f9d;"));
    placeholderLayout->addStretch();
    placeholderLayout->addWidget(line);
    auto *hint = makeLabel(tr(
        "The delay is stored in the track timestamps by mkvmerge.\n"
        "Audio packets are neither decoded nor modified."), "mutedLabel");
    hint->setWordWrap(true);
    hint->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(hint);
    placeholderLayout->addStretch();
    layout->addWidget(placeholder, 1);
    return card;
}

QWidget *MainWindow::buildActivityCard()
{
    auto *card = makeCard();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 14);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(QStringLiteral("ACTIVITY"), "sectionTitle"));
    processingLabel_ = makeLabel(tr("Idle"), "mutedLabel");
    processingLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    processingLabel_->setMinimumWidth(0);
    processingLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleRow->addWidget(processingLabel_, 1);
    layout->addLayout(titleRow);
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(false);
    layout->addWidget(progressBar_);
    logView_ = new QPlainTextEdit;
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(300);
    logView_->setMaximumHeight(105);
    layout->addWidget(logView_);
    return card;
}

void MainWindow::chooseBatchVideo()
{
    const QString initial = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString videoPath = QFileDialog::getOpenFileName(
        this, tr("Select target video"), initial, tr(kMediaFilter), nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!videoPath.isEmpty()) {
        batchVideoPath_->setText(videoPath);
    }
}

void MainWindow::chooseBatchAudio()
{
    const QString initial = batchVideoPath_->text().isEmpty()
                                ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                                : QFileInfo(batchVideoPath_->text()).absolutePath();
    const QString audioSourcePath = QFileDialog::getOpenFileName(
        this, tr("Select audio file"), initial, tr(kAudioFilter), nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!audioSourcePath.isEmpty()) {
        batchAudioPath_->setText(audioSourcePath);
    }
}

void MainWindow::analyzeCurrentPair()
{
    if (batchTable_->rowCount() == 0 || queueBusy_) {
        return;
    }

    struct AnalysisJob {
        int row;
        QString video;
        QString audio;
    };
    std::vector<AnalysisJob> jobs;
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        QTableWidgetItem *statusItem = batchTable_->item(row, 6);
        if (statusItem->data(kAnalyzedRole).toBool()) {
            continue;
        }
        const QString video = batchTable_->item(row, 1)->data(Qt::UserRole).toString();
        const QString audio = batchTable_->item(row, 2)->data(Qt::UserRole).toString();
        if (!QFileInfo::exists(video) || !QFileInfo::exists(audio)) {
            statusItem->setText(tr("Missing file"));
            processingLabel_->setText(tr("Analysis blocked"));
            appendLog(tr("A queued video or audio file no longer exists."));
            return;
        }
        jobs.push_back({row, video, audio});
        statusItem->setText(tr("Waiting…"));
    }
    if (jobs.empty()) {
        updateBatchState();
        return;
    }

    autosync_reset_cancel();
    queueBusy_ = true;
    batchButton_->setEnabled(false);
    addPairButton_->setEnabled(false);
    batchTable_->setEnabled(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    processingLabel_->setText(tr("Starting analysis…"));
    analysisResultLabel_->setText(tr("Analyzing every queued audio pair…"));
    analysisResultLabel_->setStyleSheet(QStringLiteral("color:#a9b7c2;"));
    appendLog(tr("Analyzing %1 queued file pair(s)…").arg(jobs.size()));

    const QPointer<MainWindow> window(this);
    QThread *worker = QThread::create([window, jobs = std::move(jobs)] {
        int succeeded = 0;
        const int total = static_cast<int>(jobs.size());
        for (int index = 0; index < total; ++index) {
            const AnalysisJob &job = jobs[static_cast<std::size_t>(index)];
            if (window.isNull()) {
                return;
            }
            const QByteArray videoBytes = QFile::encodeName(job.video);
            const QByteArray audioBytes = QFile::encodeName(job.audio);
            struct ProgressContext {
                QPointer<MainWindow> window;
                int index;
                int total;
            } context{window, index, total};
            const auto progressCallback = [](double progress, const char *stage, void *data) {
                auto *context = static_cast<ProgressContext *>(data);
                const QPointer<MainWindow> target = context->window;
                const double overall =
                    (static_cast<double>(context->index) + progress) /
                    static_cast<double>(context->total);
                const QString stageText = QString::fromUtf8(stage);
                QMetaObject::invokeMethod(target, [target, overall, stageText] {
                    if (!target.isNull()) {
                        target->progressBar_->setValue(
                            qBound(0, qRound(overall * 100.0), 100));
                        target->processingLabel_->setText(stageText);
                    }
                }, Qt::QueuedConnection);
            };
            QMetaObject::invokeMethod(window, [window, row = job.row] {
                if (!window.isNull() && row < window->batchTable_->rowCount()) {
                    window->batchTable_->item(row, 6)->setText(tr("Analyzing…"));
                }
            }, Qt::QueuedConnection);
            const autosync_analysis_request request = {
                videoBytes.constData(), audioBytes.constData(), nullptr,
                progressCallback, &context
            };
            autosync_analysis_summary summary{};
            const autosync_status status = autosync_analyze(&request, &summary);
            const QString message = QString::fromUtf8(summary.message);
            if (status == AUTOSYNC_STATUS_OK) {
                ++succeeded;
            }
            QMetaObject::invokeMethod(
                window, [window, row = job.row, status, summary, message] {
                    if (window.isNull() || row >= window->batchTable_->rowCount()) {
                        return;
                    }
                    QTableWidgetItem *statusItem = window->batchTable_->item(row, 6);
                    const bool success = status == AUTOSYNC_STATUS_OK;
                    statusItem->setText(success ? tr("Analyzed") : tr("Analysis failed"));
                    statusItem->setData(kAnalyzedRole, success);
                    if (success) {
                        auto *delay = qobject_cast<QDoubleSpinBox *>(
                            window->batchTable_->cellWidget(row, 5));
                        if (delay != nullptr) {
                            delay->setValue(summary.measured_offset_ms);
                        }
                    }
                    window->appendLog(message);
                }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(window, [window, succeeded, total] {
            if (window.isNull()) {
                return;
            }
            window->queueBusy_ = false;
            window->batchTable_->setEnabled(true);
            window->addPairButton_->setEnabled(
                !window->batchVideoPath_->text().trimmed().isEmpty() &&
                !window->batchAudioPath_->text().trimmed().isEmpty());
            window->progressBar_->setValue(100);
            window->processingLabel_->setText(
                succeeded == total ? tr("Analysis complete") : tr("Analysis incomplete"));
            window->analysisResultLabel_->setText(
                succeeded == total
                    ? tr("✓ All queued pairs were analyzed. Delays are ready.")
                    : tr("%1 of %2 pairs analyzed. Retry failed items.")
                          .arg(succeeded).arg(total));
            window->analysisResultLabel_->setStyleSheet(
                succeeded == total
                    ? QStringLiteral("color:#79c995; font-weight:600;")
                    : QStringLiteral("color:#e5a86b; font-weight:600;"));
            window->updateBatchState();
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::addBatchPair()
{
    const QString videoPath = batchVideoPath_->text().trimmed();
    const QString audioSourcePath = batchAudioPath_->text().trimmed();
    if (!QFileInfo::exists(videoPath) || !QFileInfo(videoPath).isFile()) {
        QMessageBox::warning(this, tr("Invalid video"),
                             tr("Select an existing video file."));
        return;
    }
    if (!QFileInfo::exists(audioSourcePath) || !QFileInfo(audioSourcePath).isFile()) {
        QMessageBox::warning(this, tr("Invalid audio"),
                             tr("Select an existing audio file."));
        return;
    }

    const int row = batchTable_->rowCount();
    batchTable_->insertRow(row);
    batchTable_->setRowHeight(row, 36);

    auto *selectionContainer = new QWidget;
    selectionContainer->setProperty("queueEditor", true);
    auto *selectionLayout = new QHBoxLayout(selectionContainer);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setAlignment(Qt::AlignCenter);
    auto *selectionCheck = new QCheckBox;
    selectionCheck->setProperty("removalCheck", true);
    selectionCheck->setProperty("queueEditor", true);
    selectionCheck->setFixedSize(17, 17);
    selectionCheck->setStyleSheet(QStringLiteral(
        "QCheckBox { margin: 0; padding: 0; spacing: 0; }"));
    selectionCheck->setToolTip(tr("Mark this task for removal"));
    selectionLayout->addWidget(selectionCheck);
    batchTable_->setCellWidget(row, 0, selectionContainer);

    auto *videoItem = new QTableWidgetItem(QFileInfo(videoPath).fileName());
    videoItem->setToolTip(videoPath);
    videoItem->setData(Qt::UserRole, videoPath);
    batchTable_->setItem(row, 1, videoItem);

    auto *audioItem = new QTableWidgetItem(QFileInfo(audioSourcePath).fileName());
    audioItem->setToolTip(audioSourcePath);
    audioItem->setData(Qt::UserRole, audioSourcePath);
    batchTable_->setItem(row, 2, audioItem);
    const QString sourceCodec = displayAudioCodec(audioSourcePath);
    auto *trackItem = new QTableWidgetItem(
        reencodeCheck_->isChecked() ? codecCombo_->currentText() : sourceCodec);
    trackItem->setData(kSourceCodecRole, sourceCodec);
    trackItem->setToolTip(reencodeCheck_->isChecked()
                              ? tr("Output codec: %1 · source: %2")
                                    .arg(codecCombo_->currentText(), sourceCodec)
                              : tr("Original codec: %1").arg(sourceCodec));
    batchTable_->setItem(row, 3, trackItem);

    auto *language = new LanguageCombo;
    language->setProperty("queueEditor", true);
    connect(language, &QComboBox::activated, this, [this] {
        QTimer::singleShot(0, batchTable_, [table = batchTable_] {
            table->clearSelection();
            table->setCurrentItem(nullptr);
        });
    });
    batchTable_->setCellWidget(row, 4, language);

    auto *delay = new DelaySpinBox;
    delay->setProperty("queueEditor", true);
    delay->setRange(-86400000, 86400000);
    delay->setDecimals(3);
    delay->setSingleStep(1.0);
    delay->setValue(batchDelay_->value());
    delay->setSuffix(QStringLiteral(" ms"));
    delay->setToolTip(tr("A positive value delays the audio; a negative value advances it."));
    batchTable_->setCellWidget(row, 5, delay);

    auto *status = new QTableWidgetItem(tr("Awaiting analysis"));
    status->setTextAlignment(Qt::AlignCenter);
    status->setData(kAnalyzedRole, false);
    batchTable_->setItem(row, 6, status);
    batchTable_->clearSelection();
    batchTable_->setCurrentItem(nullptr);
    updateBatchState();
    appendLog(tr("Job added: %1 + %2")
                  .arg(QFileInfo(videoPath).fileName(), QFileInfo(audioSourcePath).fileName()));
    batchVideoPath_->clear();
    batchAudioPath_->clear();
    batchDelay_->setValue(0);
    analysisResultLabel_->setText(tr("Select another audio file or keep the current video."));
    analysisResultLabel_->setStyleSheet(QString());
}

void MainWindow::removeSelectedJobs()
{
    if (queueBusy_) {
        return;
    }
    QList<int> rows;
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        QWidget *selectionContainer = batchTable_->cellWidget(row, 0);
        const QCheckBox *selectionCheck = selectionContainer != nullptr
                                             ? selectionContainer->findChild<QCheckBox *>()
                                             : nullptr;
        if (selectionCheck != nullptr && selectionCheck->isChecked()) {
            rows.append(row);
        }
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (const int row : rows) {
        batchTable_->removeRow(row);
    }
    updateBatchState();
}

void MainWindow::clearBatch()
{
    if (batchTable_->rowCount() == 0 || queueBusy_) {
        return;
    }
    if (QMessageBox::question(this, tr("Clear queue"),
                              tr("Remove all jobs from the queue?")) ==
        QMessageBox::Yes) {
        batchTable_->setRowCount(0);
        updateBatchState();
        appendLog(tr("Multiplexing queue cleared."));
    }
}

void MainWindow::startBatchMux()
{
    if (batchTable_->rowCount() == 0 || queueBusy_) {
        return;
    }
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        if (!batchTable_->item(row, 6)->data(kAnalyzedRole).toBool()) {
            updateBatchState();
            return;
        }
    }

    bool hasInvalidLanguage = false;
    int firstInvalidRow = -1;
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        auto *language = static_cast<LanguageCombo *>(batchTable_->cellWidget(row, 4));
        const bool invalid = language == nullptr || !language->hasValidLanguage();
        if (language != nullptr) {
            if (invalid) {
                language->flashLanguageError();
            } else {
                language->setLanguageError(false);
            }
        }
        if (invalid && firstInvalidRow < 0) {
            firstInvalidRow = row;
        }
        hasInvalidLanguage = hasInvalidLanguage || invalid;
    }
    if (hasInvalidLanguage) {
        processingLabel_->setText(tr("Invalid language tag"));
        appendLog(tr("Correct the language tags highlighted in red before processing."));
        batchTable_->scrollTo(batchTable_->model()->index(firstInvalidRow, 4));
        return;
    }

    struct BatchJob {
        int row;
        QString video;
        QString audio;
        QString output;
        QString language;
        double delayMs;
    };
    const bool audioOnly = audioFileOnlyRadio_->isChecked();
    QSet<QString> reservedOutputs;
    std::vector<BatchJob> jobs;
    jobs.reserve(static_cast<std::size_t>(batchTable_->rowCount()));
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        const QString video = batchTable_->item(row, 1)->data(Qt::UserRole).toString();
        const QString audio = batchTable_->item(row, 2)->data(Qt::UserRole).toString();
        const auto *languageCombo =
            static_cast<LanguageCombo *>(batchTable_->cellWidget(row, 4));
        const auto *delay = qobject_cast<QDoubleSpinBox *>(batchTable_->cellWidget(row, 5));
        QString language = QStringLiteral("und");
        if (languageCombo != nullptr) {
            language = languageCombo->languageCode();
        }
        const QFileInfo videoInfo(video);
        const QString outputDirectory = outputPath_->text().trimmed().isEmpty()
                                            ? videoInfo.absolutePath()
                                            : outputPath_->text().trimmed();
        const QString output = uniqueOutputPath(
            outputDirectory,
            videoInfo.completeBaseName() + QStringLiteral(".autosync"),
            audioOnly ? QStringLiteral("mka") : QStringLiteral("mkv"),
            reservedOutputs);
        jobs.push_back({row, video, audio, output, language,
                        delay != nullptr ? delay->value() : 0.0});
        batchTable_->item(row, 6)->setText(tr("Queued"));
    }

    const bool defaultTrack = defaultTrackCheck_->isChecked();
    const bool preserveChapters = preserveChaptersCheck_->isChecked();
    const bool removeTags = removeTagsCheck_->isChecked();
    const bool reencodeAudio = reencodeCheck_->isChecked();
    const QByteArray audioCodec = codecCombo_->currentData().toString().toUtf8();
    const int audioBitrate = bitrateCombo_->currentData().toInt();
    const int total = static_cast<int>(jobs.size());
    autosync_reset_cancel();
    queueBusy_ = true;
    batchButton_->setEnabled(false);
    batchTable_->setEnabled(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    processingLabel_->setText(tr("Preparing queue…"));
    appendLog(reencodeAudio
                  ? tr("Starting %1 multiplexing job(s) with audio encoding.").arg(total)
                  : tr("Starting %1 multiplexing job(s) without re-encoding.").arg(total));

    const QPointer<MainWindow> window(this);
    QThread *worker = QThread::create(
        [window, jobs = std::move(jobs), defaultTrack, preserveChapters, removeTags,
         audioOnly, reencodeAudio, audioCodec, audioBitrate, total] {
            int succeeded = 0;
            for (std::size_t jobIndex = 0; jobIndex < jobs.size(); ++jobIndex) {
                const BatchJob &job = jobs[jobIndex];
                if (window.isNull()) {
                    return;
                }
                const QByteArray video = QFile::encodeName(job.video);
                const QByteArray audio = QFile::encodeName(job.audio);
                const QByteArray language = job.language.toUtf8();
                const QString selectedOutput = job.output;
                const QByteArray output = QFile::encodeName(selectedOutput);
                struct MuxProgressContext {
                    QPointer<MainWindow> window;
                    std::size_t jobIndex;
                    int total;
                } progressContext{window, jobIndex, total};
                const auto muxProgressCallback = [](double progress, const char *stage,
                                                    void *data) {
                    auto *context = static_cast<MuxProgressContext *>(data);
                    const QPointer<MainWindow> target = context->window;
                    const double overall =
                        (static_cast<double>(context->jobIndex) + progress) /
                        static_cast<double>(context->total);
                    const QString text = QString::fromUtf8(stage);
                    QMetaObject::invokeMethod(target, [target, overall, text] {
                        if (!target.isNull()) {
                            target->progressBar_->setValue(
                                qBound(0, qRound(overall * 100.0), 100));
                            target->processingLabel_->setText(text);
                        }
                    }, Qt::QueuedConnection);
                };
                const autosync_mux_request request = {
                    video.constData(), audio.constData(), output.constData(), job.delayMs,
                    language.constData(), defaultTrack ? 1 : 0,
                    preserveChapters ? 1 : 0, removeTags ? 1 : 0,
                    audioOnly ? 1 : 0,
                    reencodeAudio ? 1 : 0, audioCodec.constData(), audioBitrate,
                    muxProgressCallback, &progressContext
                };
                autosync_mux_result result{};
                QMetaObject::invokeMethod(window, [window, row = job.row] {
                    if (!window.isNull()) {
                        window->batchTable_->item(row, 6)->setText(tr("Processing"));
                    }
                }, Qt::QueuedConnection);
                const autosync_status status = autosync_mux(&request, &result);
                if (status == AUTOSYNC_STATUS_OK) {
                    ++succeeded;
                }
                const QString message = QString::fromUtf8(result.message);
                QMetaObject::invokeMethod(
                    window, [window, row = job.row, status, message,
                             outputPath = selectedOutput,
                             completed = job.row + 1, total] {
                        if (window.isNull()) {
                            return;
                        }
                        window->batchTable_->item(row, 6)->setText(
                            status == AUTOSYNC_STATUS_OK ? tr("Complete") : tr("Error"));
                        window->progressBar_->setValue((completed * 100) / total);
                        window->appendLog(status == AUTOSYNC_STATUS_OK
                                              ? tr("Completed: %1").arg(outputPath)
                                              : tr("Failed: %1").arg(message));
                    }, Qt::QueuedConnection);
            }
            QMetaObject::invokeMethod(window, [window, succeeded, total] {
                if (window.isNull()) {
                    return;
                }
                window->batchTable_->setEnabled(true);
                window->queueBusy_ = false;
                window->batchButton_->setEnabled(window->batchTable_->rowCount() > 0);
                window->progressBar_->setValue(100);
                window->processingLabel_->setText(tr("Multiplexing complete"));
                window->appendLog(tr("Processing finished: %1 of %2 file(s) created.")
                                      .arg(succeeded).arg(total));
            }, Qt::QueuedConnection);
        });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void MainWindow::updateBatchState()
{
    const int count = batchTable_->rowCount();
    bool allAnalyzed = count > 0;
    for (int row = 0; row < count; ++row) {
        allAnalyzed = allAnalyzed &&
                      batchTable_->item(row, 6)->data(kAnalyzedRole).toBool();
    }
    jobsLabel_->setText(count == 1 ? tr("1 job") : tr("%1 jobs").arg(count));
    anchorLabel_->setText(QString::number(count));
    batchButton_->setText(allAnalyzed ? tr("START PROCESSING") : tr("ANALYZE FILES"));
    batchButton_->setEnabled(count > 0 && !queueBusy_);
}

void MainWindow::updateQueueTrackLabels()
{
    const bool reencode = reencodeCheck_ != nullptr && reencodeCheck_->isChecked();
    const QString outputCodec = codecCombo_ != nullptr
                                    ? codecCombo_->currentText()
                                    : QString();
    for (int row = 0; row < batchTable_->rowCount(); ++row) {
        QTableWidgetItem *trackItem = batchTable_->item(row, 3);
        if (trackItem == nullptr) {
            continue;
        }
        const QString sourceCodec = trackItem->data(kSourceCodecRole).toString();
        trackItem->setText(reencode ? outputCodec : sourceCodec);
        trackItem->setToolTip(reencode
                                  ? tr("Output codec: %1 · source: %2")
                                        .arg(outputCodec, sourceCodec)
                                  : tr("Original codec: %1").arg(sourceCodec));
    }
}

void MainWindow::updateCpuUsage()
{
    const SystemMetrics metrics = systemMonitor_.sample();
    cpuUsageLabel_->setText(metrics.cpuUsagePercent >= 0
                                ? tr("%1%").arg(metrics.cpuUsagePercent)
                                : QStringLiteral("--%"));
    memoryUsageLabel_->setText(metrics.memoryUsagePercent >= 0
                                   ? tr("%1%").arg(metrics.memoryUsagePercent)
                                   : QStringLiteral("--%"));
    cpuTemperatureLabel_->setText(metrics.cpuTemperatureCelsius >= 0
                                      ? tr("%1°C").arg(metrics.cpuTemperatureCelsius)
                                      : QStringLiteral("--°C"));
}

void MainWindow::appendLog(const QString &message)
{
    if (logView_ != nullptr) {
        logView_->appendPlainText(QStringLiteral("› %1").arg(message));
    }
}

void MainWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    outputPath_->setText(settings.value(QStringLiteral("paths/output")).toString());
    themeCombo_->setCurrentIndex(settings.value(QStringLiteral("ui/theme"), 0).toInt());
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("paths/output"), outputPath_->text());
    settings.setValue(QStringLiteral("ui/theme"), themeCombo_->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (queueBusy_) {
        autosync_request_cancel();
        processingLabel_->setText(tr("Cancelling…"));
    }
    saveSettings();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (batchTable_ != nullptr && event->type() == QEvent::MouseButtonPress) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        while (widget != nullptr) {
            if (widget->property("queueEditor").toBool()) {
                QTimer::singleShot(0, batchTable_, [table = batchTable_] {
                    table->clearSelection();
                    table->setCurrentItem(nullptr);
                });
                break;
            }
            widget = widget->parentWidget();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
