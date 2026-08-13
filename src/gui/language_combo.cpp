#include "language_combo.h"

#include <QAbstractItemView>
#include <QLineEdit>
#include <QLocale>
#include <QPaintEvent>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QValidator>

namespace {

QSet<QString> supportedLanguageCodes()
{
    QSet<QString> codes{QStringLiteral("und")};
    QProcess process;
    process.start(QStringLiteral("mkvmerge"), {QStringLiteral("--list-languages")});
    if (!process.waitForFinished(3000)) {
        return codes;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QRegularExpression codePattern(QStringLiteral(
        R"(^.+\|\s*([a-z]{3})\s*\|\s*([a-z]{3})?\s*\|\s*([a-z]{2})?\s*$)"));
    for (const QString &line : output.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = codePattern.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        for (int index = 1; index <= 3; ++index) {
            if (!match.captured(index).isEmpty()) {
                codes.insert(match.captured(index));
            }
        }
    }
    return codes;
}

QSet<QString> supportedLanguageRegions()
{
    QSet<QString> combinations;
    const QList<QLocale> locales = QLocale::matchingLocales(
        QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
    for (const QLocale &locale : locales) {
        const QRegularExpressionMatch match =
            QRegularExpression(QStringLiteral("^([a-z]{2})-([A-Z]{2})$"))
                .match(locale.bcp47Name());
        if (match.hasMatch()) {
            combinations.insert((match.captured(1) + QLatin1Char('-') +
                                 match.captured(2)).toLower());
        }
    }
    return combinations;
}

class LanguageTagValidator final : public QValidator
{
public:
    explicit LanguageTagValidator(QObject *parent = nullptr)
        : QValidator(parent), supported_(supportedLanguageCodes()),
          languageRegions_(supportedLanguageRegions())
    {
    }

    State validate(QString &input, int &position) const override
    {
        Q_UNUSED(position)
        if (input.isEmpty()) {
            return Intermediate;
        }
        if (!QRegularExpression(QStringLiteral("^[A-Za-z-]{0,5}$"))
                 .match(input).hasMatch()) {
            return Invalid;
        }

        const QString lower = input.toLower();
        if (QRegularExpression(QStringLiteral("^[a-z]{3}$"))
                .match(lower).hasMatch()) {
            return supported_.contains(lower) ? Acceptable : Intermediate;
        }
        const QRegularExpressionMatch regional =
            QRegularExpression(QStringLiteral("^([a-z]{2})-([a-z]{2})$"))
                .match(lower);
        if (regional.hasMatch()) {
            return languageRegions_.contains(lower) ? Acceptable : Intermediate;
        }
        if (QRegularExpression(QStringLiteral("^[a-z]{0,2}(?:-[a-z]{0,2})?$"))
                .match(lower).hasMatch()) {
            return Intermediate;
        }
        return Invalid;
    }

    void fixup(QString &input) const override
    {
        input = input.trimmed();
        const QRegularExpressionMatch regional =
            QRegularExpression(QStringLiteral("^([A-Za-z]{2})-([A-Za-z]{2})$"))
                .match(input);
        if (regional.hasMatch()) {
            input = regional.captured(1).toLower() + QLatin1Char('-') +
                    regional.captured(2).toUpper();
        } else {
            input = input.toLower();
        }
    }

private:
    const QSet<QString> supported_;
    const QSet<QString> languageRegions_;
};

} // namespace

LanguageCombo::LanguageCombo(QWidget *parent)
    : QComboBox(parent)
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setCompleter(nullptr);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(3);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addItem(QStringLiteral("und"), QStringLiteral("und"));
    addItem(tr("eng — English"), QStringLiteral("eng"));
    addItem(tr("fra — French"), QStringLiteral("fra"));
    addItem(tr("deu — German"), QStringLiteral("deu"));
    addItem(tr("ita — Italian"), QStringLiteral("ita"));
    addItem(tr("jpn — Japanese"), QStringLiteral("jpn"));
    addItem(tr("por — Portuguese"), QStringLiteral("por"));
    addItem(tr("spa — Spanish"), QStringLiteral("spa"));
    setCurrentIndex(0);
    connect(this, &QComboBox::activated, this, [this](int index) {
        if (index >= 0) {
            lineEdit()->setText(itemData(index).toString());
            lineEdit()->setCursorPosition(0);
        }
    });
    view()->setMinimumWidth(220);
    lineEdit()->setClearButtonEnabled(false);
    lineEdit()->setPlaceholderText(QStringLiteral("und"));
    lineEdit()->setValidator(new LanguageTagValidator(lineEdit()));
    connect(lineEdit(), &QLineEdit::editingFinished, this, [this] {
        QString tag = lineEdit()->text();
        const QValidator *validator = lineEdit()->validator();
        int position = tag.size();
        if (validator != nullptr &&
            validator->validate(tag, position) == QValidator::Acceptable) {
            validator->fixup(tag);
            lineEdit()->setText(tag);
        }
    });
    connect(lineEdit(), &QLineEdit::textEdited, this, [this] {
        if (hasValidLanguage()) {
            setLanguageError(false);
        }
    });
    setToolTip(tr(
        "Use a valid ISO language code (eng, por, jpn) or language-region tag "
        "(en-US, pt-BR)."));
}

QString LanguageCombo::languageCode() const
{
    if (currentIndex() >= 0 && currentText() == itemText(currentIndex())) {
        return currentData().toString();
    }
    const QString typed = currentText().trimmed();
    return typed.isEmpty() ? QStringLiteral("und") : typed;
}

bool LanguageCombo::hasValidLanguage() const
{
    QString tag = currentText().trimmed();
    int position = tag.size();
    return lineEdit()->validator() != nullptr &&
           lineEdit()->validator()->validate(tag, position) == QValidator::Acceptable;
}

void LanguageCombo::setLanguageError(bool invalid)
{
    setProperty("invalidLanguage", invalid);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void LanguageCombo::flashLanguageError()
{
    setLanguageError(true);
    QTimer::singleShot(150, this, [this] { setLanguageError(false); });
    QTimer::singleShot(280, this, [this] { setLanguageError(true); });
    QTimer::singleShot(430, this, [this] { setLanguageError(false); });
    QTimer::singleShot(560, this, [this] { setLanguageError(true); });
    QTimer::singleShot(760, this, [this] { setLanguageError(false); });
}

void LanguageCombo::paintEvent(QPaintEvent *event)
{
    QComboBox::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#aeb7bd")), 1.5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal x = width() - 14.0;
    const qreal y = height() / 2.0 - 1.0;
    painter.drawLine(QPointF(x - 3.5, y), QPointF(x, y + 3.5));
    painter.drawLine(QPointF(x, y + 3.5), QPointF(x + 3.5, y));
}
