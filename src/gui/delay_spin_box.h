#pragma once

#include <QDoubleSpinBox>

class DelaySpinBox final : public QDoubleSpinBox
{
public:
    explicit DelaySpinBox(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};
