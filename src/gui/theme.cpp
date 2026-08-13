#include "theme.h"

namespace AutoSyncTheme {

QString darkStyleSheet()
{
    return QStringLiteral(R"QSS(
        * {
            font-family: "Inter", "Noto Sans", "DejaVu Sans", sans-serif;
            font-size: 13px;
            color: #d8d8d8;
        }

        QMainWindow, QWidget#root, QWidget#workspace {
            background: #202020;
        }

        QWidget#sidebar {
            background: #191919;
            border-right: 1px solid #303030;
        }

        QLabel#appMark {
            color: #f1f1f1;
            background: #252525;
            border: 1px solid #3a3a3a;
            border-radius: 8px;
            font-size: 12px;
            font-weight: 800;
        }

        QLabel#eyebrow, QLabel#mutedLabel {
            color: #858585;
            font-size: 11px;
            font-weight: 500;
        }

        QLabel#pageTitle {
            color: #ededed;
            font-size: 16px;
            font-weight: 600;
        }

        QLabel#sectionTitle {
            color: #b9b9b9;
            font-size: 11px;
            font-weight: 700;
        }

        QLabel#recommendation {
            color: #74c7ec;
            font-size: 16px;
            font-weight: 650;
        }

        QFrame#cpuIndicator, QFrame#temperatureIndicator, QFrame#memoryIndicator {
            background: #282828;
            border: 1px solid #373737;
            border-radius: 3px;
        }

        QFrame#cpuIndicator QLabel, QFrame#temperatureIndicator QLabel,
        QFrame#memoryIndicator QLabel {
            border: 0;
            background: transparent;
            color: #bdbdbd;
            font-size: 11px;
            font-weight: 600;
        }

        QFrame#card, QFrame#metricCard {
            background: #252525;
            border: 1px solid #323232;
            border-radius: 4px;
        }

        QFrame#headerCard {
            background: #202020;
            border: 0;
            border-bottom: 1px solid #303030;
            border-radius: 0;
        }

        QFrame#metricCard {
            background: #212121;
            border-color: #303030;
        }

        QPushButton {
            min-height: 34px;
            padding: 0 14px;
            background: #303030;
            border: 1px solid #404040;
            border-radius: 3px;
            color: #dedede;
            font-weight: 500;
        }

        QPushButton:hover {
            background: #383838;
            border-color: #505050;
        }

        QPushButton:pressed {
            background: #292929;
        }

        QPushButton:disabled {
            color: #686868;
            background: #292929;
            border-color: #333333;
        }

        QPushButton#primaryButton {
            background: #315e78;
            border-color: #477f9d;
            color: #f5f5f5;
        }

        QPushButton#primaryButton:hover {
            background: #39718f;
        }

        QPushButton#primaryButton:disabled {
            color: #707070;
            background: #292929;
            border-color: #353535;
        }

        QPushButton#navButton {
            min-width: 52px;
            max-width: 52px;
            min-height: 48px;
            max-height: 48px;
            padding: 0;
            text-align: center;
            border: 0;
            border-left: 3px solid transparent;
            border-radius: 0;
            background: transparent;
            color: #a5a5a5;
            font-size: 20px;
            font-weight: 400;
        }

        QPushButton#navButton:hover {
            background: #252525;
            color: #eeeeee;
        }

        QPushButton#navButton:checked {
            background: #292929;
            color: #74c7ec;
            border-left-color: #74c7ec;
        }

        QPushButton#sideAction {
            min-width: 46px;
            max-width: 46px;
            min-height: 42px;
            max-height: 42px;
            padding: 0;
            border: 0;
            background: transparent;
            color: #a4a4a4;
            font-size: 18px;
        }

        QPushButton#sideAction:hover {
            background: #292929;
            color: #efefef;
        }

        QLineEdit, QComboBox {
            min-height: 34px;
            padding: 0 10px;
            background: #1d1d1d;
            border: 1px solid #383838;
            border-radius: 3px;
            selection-background-color: #3b708d;
        }

        QLineEdit:focus, QComboBox:focus {
            border-color: #6096b4;
        }

        QComboBox[invalidLanguage="true"] {
            border: 1px solid #d45f5f;
        }

        QComboBox::drop-down {
            border: 0;
            width: 26px;
        }

        QCheckBox, QRadioButton {
            spacing: 8px;
            color: #c8c8c8;
        }

        QCheckBox::indicator, QRadioButton::indicator {
            width: 15px;
            height: 15px;
        }

        QCheckBox::indicator {
            border: 1px solid #606060;
            border-radius: 2px;
            background: #1c1c1c;
        }

        QCheckBox::indicator:checked {
            background: #548baa;
            border-color: #70a9c8;
        }

        QRadioButton::indicator {
            border: 1px solid #606060;
            border-radius: 2px;
            background: #1c1c1c;
        }

        QRadioButton::indicator:checked {
            background: #548baa;
            border: 1px solid #70a9c8;
        }

        QPlainTextEdit {
            background: #1c1c1c;
            border: 1px solid #343434;
            border-radius: 3px;
            color: #a9b7c2;
            padding: 7px;
            font-family: "JetBrains Mono", "DejaVu Sans Mono", monospace;
            font-size: 11px;
        }

        QTableWidget {
            background: #1d1d1d;
            alternate-background-color: #222222;
            border: 1px solid #383838;
            border-radius: 3px;
            gridline-color: #343434;
            selection-background-color: #1d1d1d;
            selection-color: #dedede;
        }

        QTableWidget::item:selected {
            background: #1d1d1d;
            color: #dedede;
        }

        QHeaderView::section {
            min-height: 30px;
            padding: 0 8px;
            background: #292929;
            border: 0;
            border-right: 1px solid #3a3a3a;
            border-bottom: 1px solid #3a3a3a;
            color: #bcbcbc;
            font-size: 11px;
            font-weight: 600;
        }

        QTableCornerButton::section {
            background: #292929;
            border: 0;
        }

        QSpinBox, QDoubleSpinBox {
            min-height: 28px;
            padding: 0 22px 0 6px;
            background: #202020;
            border: 1px solid #3c3c3c;
            border-radius: 2px;
        }

        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            width: 20px;
            background: transparent;
            border: 0;
        }

        QDoubleSpinBox::up-button {
            subcontrol-position: top right;
        }

        QDoubleSpinBox::down-button {
            subcontrol-position: bottom right;
        }

        QDoubleSpinBox::up-arrow, QDoubleSpinBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
        }

        QProgressBar {
            min-height: 8px;
            max-height: 8px;
            background: #343434;
            border: 0;
            border-radius: 2px;
            text-align: center;
        }

        QProgressBar::chunk {
            background: #6aa7c7;
            border-radius: 2px;
        }

        QScrollArea {
            border: 0;
            background: transparent;
        }

        QScrollBar:vertical {
            width: 9px;
            background: #202020;
        }

        QScrollBar::handle:vertical {
            min-height: 30px;
            background: #484848;
            border-radius: 3px;
        }

        QToolTip {
            color: #eeeeee;
            background: #303030;
            border: 1px solid #505050;
            padding: 5px;
        }
    )QSS");
}

} // namespace AutoSyncTheme
