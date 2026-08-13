#include "delay_spin_box.h"

#include <QPaintEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSpinBox>

DelaySpinBox::DelaySpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{
    setButtonSymbols(QAbstractSpinBox::UpDownArrows);
}

void DelaySpinBox::paintEvent(QPaintEvent *event)
{
    QDoubleSpinBox::paintEvent(event);

    QStyleOptionSpinBox option;
    initStyleOption(&option);
    const QRect up = style()->subControlRect(
        QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, this);
    const QRect down = style()->subControlRect(
        QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, this);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#aeb7bd")), 1.4,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    const auto drawChevron = [&painter](const QRect &area, bool pointsUp) {
        const QPointF center = area.center();
        const qreal direction = pointsUp ? -1.0 : 1.0;
        painter.drawLine(QPointF(center.x() - 3.0, center.y() - direction * 1.5),
                         QPointF(center.x(), center.y() + direction * 1.5));
        painter.drawLine(QPointF(center.x(), center.y() + direction * 1.5),
                         QPointF(center.x() + 3.0, center.y() - direction * 1.5));
    };
    drawChevron(up, true);
    drawChevron(down, false);
}
