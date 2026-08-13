#pragma once

#include <QComboBox>

class LanguageCombo final : public QComboBox
{
public:
    explicit LanguageCombo(QWidget *parent = nullptr);

    QString languageCode() const;
    bool hasValidLanguage() const;
    void setLanguageError(bool invalid);
    void flashLanguageError();

protected:
    void paintEvent(QPaintEvent *event) override;
};
