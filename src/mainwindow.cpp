// mainwindow.cpp
#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(central);

    QWidget *sidePanel = new QWidget(central);
    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);

    btnSelect = new QPushButton("Выделение", sidePanel);
    btnHand   = new QPushButton("Рука", sidePanel);
    btnLine   = new QPushButton("Линия", sidePanel);
    btnRect   = new QPushButton("Квадрат", sidePanel);
    btnCircle = new QPushButton("Круг", sidePanel);

    sideLayout->addWidget(btnSelect);
    sideLayout->addWidget(btnHand);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(btnLine);
    sideLayout->addWidget(btnRect);
    sideLayout->addWidget(btnCircle);
    sideLayout->addStretch();

    canvas = new Canvas(central);

    layout->addWidget(sidePanel);
    layout->addWidget(canvas, 1);
    setCentralWidget(central);

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
}
