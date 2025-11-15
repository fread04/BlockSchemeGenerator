#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <vector>
#include <QMap>
#include <QTransform>

// Enums
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

// All 8 sides where "drag handles" can be + none
enum class HandlePosition {
    None,
    TopLeft, Top, TopRight,          // Top
    Left, Right,                     // Middle
    BottomLeft, Bottom, BottomRight, // Bottom
    Start, End                       // For lines
};

struct Shape {
    ShapeType type;
    QRect rect;            // For shapes
    QPoint start;          // For line
    QPoint end;            // For line
    bool selected = false;

    // Temp copy of stats of origin shape for resize
    QRect originalRect;
    QPoint originalStart;
    QPoint originalEnd;

    // Function to get selection border
    QRectF bounds() const {
        if (type == ShapeType::Line) {
            return QRectF(start, end).normalized();
        }
        return QRectF(rect);
    }
};


class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);

    // Setters
    void setShapeType(ShapeType type) { currentShape = type; };
    void setTool(Tool tool) { currentTool = tool; updateCursorIcon(); }

protected:
    void paintEvent(QPaintEvent *) override; //

    // Mouse and key events
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Current selected states
    Tool currentTool = Tool::Select;
    ShapeType currentShape = ShapeType::Line;

    // Current action states
    bool drawing = false;
    bool moving = false;
    bool resizing = false;
    bool selecting = false;

    // Start and end of painting
    QPoint startPoint;
    QPoint lastMousePos;

    QRect selectionRect;

    std::vector<Shape> shapes; // Array of all shapes

    // Resize
    Shape* resizingShape = nullptr; // Main resize shape
    HandlePosition currentResizeHandle = HandlePosition::None;
    std::vector<Shape> originalShapes;

    void applyResize(const QPoint& mousePos, Qt::KeyboardModifiers modifiers);
    QPointF getAnchorPoint(const QRectF& rect, HandlePosition handle, bool fromCenter);
    QPointF scalePoint(const QPointF& p, const QPointF& anchor, qreal sx, qreal sy);

    // Detecction
    Shape* shapeAt(const QPoint &pos);
    std::pair<Shape*, HandlePosition> getHandleAt(const QPoint& pos);
    QMap<HandlePosition, QRectF> getResizeHandles(const Shape& s) const;

    // Cursor change
    void updateCursorIcon(const QPoint &pos = QPoint());
};

#endif // CANVAS_H
