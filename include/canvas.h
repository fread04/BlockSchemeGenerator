#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>

enum class Tool {
    Draw,
    Select,
    Hand
};

enum class ShapeType {
    Line,
    Rectangle,
    Circle
};

struct Shape {
    ShapeType type;
    QRect rect;        // для прямоугольника и круга
    QPoint start;      // для линии
    QPoint end;        // для линии
    bool selected = false;
};


class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);
    void setShapeType(ShapeType type);
    void setTool(Tool tool) { currentTool = tool; }

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Tool currentTool = Tool::Draw;
    ShapeType currentShape = ShapeType::Line;
    Shape* selectedShape = nullptr;
    bool drawing = false;
    bool moving = false;
    QPoint startPoint;
    QPoint offset; // смещение при перетаскивании
    Shape *currentMovingShape = nullptr;
    std::vector<Shape> shapes;
    QPoint lastMousePos;

    static qreal distancePointToLine(const QPointF &p, const QPointF &a, const QPointF &b);
    Shape* shapeAt(const QPoint &pos);
};

#endif // CANVAS_H
