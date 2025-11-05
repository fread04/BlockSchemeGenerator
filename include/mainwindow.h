// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include "canvas.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    Canvas *canvas;
    QPushButton *btnSelect;
    QPushButton *btnHand;
    QPushButton *btnLine;
    QPushButton *btnRect;
    QPushButton *btnCircle;
};

#endif // MAINWINDOW_H
