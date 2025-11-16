#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox> // (ДОБАВЛЕНО)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(central);

    QWidget *sidePanel = new QWidget(central);
    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);

    // --- Кнопки Инструментов ---
    btnSelect = new QPushButton("Выделение", sidePanel);
    btnHand   = new QPushButton("Рука", sidePanel);
    btnLine   = new QPushButton("Линия", sidePanel);
    btnRect   = new QPushButton("Квадрат", sidePanel);
    btnCircle = new QPushButton("Круг", sidePanel);

    // --- (НОВОЕ) Галочки Настроек ---
    chkGrid = new QCheckBox("Сетка", sidePanel);
    chkSnap = new QCheckBox("Привязка", sidePanel);

    chkGrid->setChecked(true); // Включаем по умолчанию
    chkSnap->setChecked(true); // Включаем по умолчанию


    // --- Добавляем виджеты на панель ---
    sideLayout->addWidget(btnSelect);
    sideLayout->addWidget(btnHand);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(btnLine);
    sideLayout->addWidget(btnRect);
    sideLayout->addWidget(btnCircle);
    sideLayout->addSpacing(20); // (ДОБАВЛЕН Отступ)
    sideLayout->addWidget(chkGrid); // (ДОБАВЛЕНО)
    sideLayout->addWidget(chkSnap); // (ДОБАВЛЕНО)
    sideLayout->addStretch();

    // --- Холст ---
    canvas = new Canvas(central);

    // --- Сборка layout ---
    layout->addWidget(sidePanel);
    layout->addWidget(canvas, 1); // 1 = stretch factor
    setCentralWidget(central);

    // --- Соединения (Connects) ---

    // Инструменты
    connect(btnSelect, &QPushButton::clicked, this, [this]() { canvas->setTool(Tool::Select); });
    connect(btnHand,   &QPushButton::clicked, this, [this]() { canvas->setTool(Tool::Hand); });
    connect(btnLine,   &QPushButton::clicked, this, [this]() {
        canvas->setTool(Tool::Draw);
        canvas->setShapeType(ShapeType::Line);
    });
    connect(btnRect,   &QPushButton::clicked, this, [this]() {
        canvas->setTool(Tool::Draw);
        canvas->setShapeType(ShapeType::Rectangle);
    });
    connect(btnCircle, &QPushButton::clicked, this, [this]() {
        canvas->setTool(Tool::Draw);
        canvas->setShapeType(ShapeType::Circle);
    });

    // (НОВЫЕ) Соединения для галочек
    // (Используем QCheckBox::toggled, а не ::clicked)
    connect(chkGrid, &QCheckBox::toggled,
            canvas, &Canvas::setGridEnabled);

    connect(chkSnap, &QCheckBox::toggled,
            canvas, &Canvas::setSnapEnabled);
}
