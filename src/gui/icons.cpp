#include "icons.h"

#include <QColor>
#include <QFont>
#include <QPainter>

namespace autosync::icons {
namespace {

constexpr int kMetricIconSize = 18;
const QColor kMetricColor(QStringLiteral("#aeb7bd"));
const QColor kSidebarColor(QStringLiteral("#b9c1c6"));
const QColor kActiveColor(QStringLiteral("#62c9ef"));

QPixmap drawSynchronize(const QColor &color)
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(3, 7), QPointF(16, 7));
    painter.drawLine(QPointF(13, 4), QPointF(16, 7));
    painter.drawLine(QPointF(16, 7), QPointF(13, 10));
    painter.drawLine(QPointF(17, 13), QPointF(4, 13));
    painter.drawLine(QPointF(7, 10), QPointF(4, 13));
    painter.drawLine(QPointF(4, 13), QPointF(7, 16));
    return pixmap;
}

} // namespace

QPixmap cpu()
{
    QPixmap pixmap(kMetricIconSize, kMetricIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kMetricColor, 1.4));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(4.5, 4.5, 9, 9), 1.5, 1.5);
    painter.drawRect(QRectF(7, 7, 4, 4));
    for (const qreal position : {6.0, 9.0, 12.0}) {
        painter.drawLine(QPointF(position, 2), QPointF(position, 4.5));
        painter.drawLine(QPointF(position, 13.5), QPointF(position, 16));
        painter.drawLine(QPointF(2, position), QPointF(4.5, position));
        painter.drawLine(QPointF(13.5, position), QPointF(16, position));
    }
    return pixmap;
}

QPixmap memory()
{
    QPixmap pixmap(kMetricIconSize, kMetricIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kMetricColor, 1.4, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(4.5, 4.5, 9, 9), 1.5, 1.5);
    for (const qreal position : {6.0, 9.0, 12.0}) {
        painter.drawLine(QPointF(position, 2), QPointF(position, 4.5));
        painter.drawLine(QPointF(position, 13.5), QPointF(position, 16));
        painter.drawLine(QPointF(2, position), QPointF(4.5, position));
        painter.drawLine(QPointF(13.5, position), QPointF(16, position));
    }
    painter.setBrush(kMetricColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(6.5, 6.5, 2, 5), 0.5, 0.5);
    painter.drawRoundedRect(QRectF(9.5, 6.5, 2, 5), 0.5, 0.5);
    return pixmap;
}

QPixmap temperature()
{
    QPixmap pixmap(kMetricIconSize, kMetricIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kMetricColor, 1.5, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(7, 2, 4, 10), 2, 2);
    painter.drawLine(QPointF(9, 5), QPointF(9, 13));
    painter.setBrush(kMetricColor);
    painter.drawEllipse(QRectF(5.5, 10, 7, 7));
    return pixmap;
}

QIcon bug()
{
    QPixmap pixmap(kMetricIconSize, kMetricIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kSidebarColor, 1.4, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(5, 5, 8, 10));
    painter.drawLine(QPointF(7, 5), QPointF(6, 3));
    painter.drawLine(QPointF(11, 5), QPointF(12, 3));
    painter.drawLine(QPointF(6, 8), QPointF(3, 6.5));
    painter.drawLine(QPointF(12, 8), QPointF(15, 6.5));
    painter.drawLine(QPointF(6, 11), QPointF(3, 11));
    painter.drawLine(QPointF(12, 11), QPointF(15, 11));
    painter.drawLine(QPointF(6, 14), QPointF(3.5, 16));
    painter.drawLine(QPointF(12, 14), QPointF(14.5, 16));
    painter.drawLine(QPointF(9, 5), QPointF(9, 15));
    return QIcon(pixmap);
}

QIcon about()
{
    QPixmap pixmap(kMetricIconSize, kMetricIconSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kSidebarColor, 1.5, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(2.5, 2.5, 13, 13));
    painter.setBrush(kSidebarColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(8.1, 5.1, 1.8, 1.8));
    painter.drawRoundedRect(QRectF(8.1, 8, 1.8, 5), 0.9, 0.9);
    return QIcon(pixmap);
}

QIcon synchronize()
{
    QIcon icon;
    icon.addPixmap(drawSynchronize(kSidebarColor), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawSynchronize(kActiveColor), QIcon::Normal, QIcon::On);
    return icon;
}

QIcon unavailable()
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = painter.font();
    font.setPixelSize(15);
    font.setWeight(QFont::Medium);
    painter.setFont(font);
    painter.setPen(kSidebarColor);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("?"));
    return QIcon(pixmap);
}

} // namespace autosync::icons
