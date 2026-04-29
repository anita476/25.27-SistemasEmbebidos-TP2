
LIGHT_THEME = """
QWidget {
    background-color: #ffffff;
    color: #000000;
}
QPushButton {
    background-color: #f0f0f0;
    color: #000000;
    border: 1px solid #cccccc;
    border-radius: 4px;
    padding: 5px;
}
QPushButton:hover {
    background-color: #e0e0e0;
}
QPushButton:pressed {
    background-color: #cccccc;
}
QPushButton:disabled {
    background-color: #cccccc;
    color: #666666;
}
QLabel {
    color: #000000;
}
QLabel:disabled {
    color: #666666;
}
QLineEdit, QTextEdit {
    background-color: #ffffff;
    color: #000000;
    border: 1px solid #cccccc;
    border-radius: 4px;
}
QTextEdit a {
    color: #0000ff;
    text-decoration: underline;
}
QCheckBox {
    color: #000000;
}
QCheckBox::indicator {
    width: 12px;
    height: 12px;
    border: 1px solid #cccccc;
    border-radius: 2px;
    background-color: #ffffff;
}
QCheckBox::indicator:checked {
    background-color: #007bff;
    border: 1px solid #007bff;
}
QCheckBox:disabled {
    color: #666666;
}
QGroupBox {
    color: #000000;
    border: 1px solid #cccccc;
    border-radius: 4px;
    margin-top: 1ex;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px 0 5px;
}
QGroupBox:disabled {
    color: #666666;
    border-color: #cccccc;
}
QComboBox {
    background-color: #ffffff;
    color: #000000;
    border: 1px solid #cccccc;
    border-radius: 4px;
    padding: 5px;
}
QComboBox:hover {
    border-color: #999999;
}
QComboBox::drop-down {
    border: none;
}
QComboBox::down-arrow {
    width: 12px;
    height: 12px;
}
QComboBox QAbstractItemView {
    background-color: #ffffff;
    color: #000000;
    border: 1px solid #cccccc;
    selection-background-color: #e0e0e0;
}
QComboBox:disabled {
    background-color: #cccccc;
    color: #666666;
    border-color: #cccccc;
}
"""

DARK_THEME = """
QWidget {
    background-color: #2b2b2b;
    color: #ffffff;
}
QPushButton {
    background-color: #555555;
    color: #ffffff;
    border: 1px solid #777777;
    border-radius: 4px;
    padding: 5px;
}
QPushButton:hover {
    background-color: #666666;
}
QPushButton:pressed {
    background-color: #333333;
}
QPushButton:disabled {
    background-color: #3c3c3c;
    color: #999999;
}
QLabel {
    color: #ffffff;
}
QLabel:disabled {
    color: #999999;
}
QLineEdit, QTextEdit {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #777777;
    border-radius: 4px;
}
QTextEdit a {
    color: #add8e6;
    text-decoration: underline;
}
QCheckBox {
    color: #ffffff;
}
QCheckBox::indicator {
    width: 12px;
    height: 12px;
    border: 1px solid #777777;
    border-radius: 2px;
    background-color: #3c3c3c;
}
QCheckBox::indicator:checked {
    background-color: #007bff;
    border: 1px solid #007bff;
}
QCheckBox:disabled {
    color: #999999;
}
QGroupBox {
    color: #ffffff;
    border: 1px solid #777777;
    border-radius: 4px;
    margin-top: 1ex;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px 0 5px;
}
QGroupBox:disabled {
    color: #999999;
    border-color: #777777;
}
QComboBox {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #777777;
    border-radius: 4px;
    padding: 5px;
}
QComboBox:hover {
    border-color: #999999;
}
QComboBox::drop-down {
    border: none;
}
QComboBox::down-arrow {
    width: 12px;
    height: 12px;
}
QComboBox QAbstractItemView {
    background-color: #3c3c3c;
    color: #ffffff;
    border: 1px solid #777777;
    selection-background-color: #555555;
}
QComboBox:disabled {
    background-color: #2b2b2b;
    color: #999999;
    border-color: #777777;
}
"""
