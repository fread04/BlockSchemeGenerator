#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <vector>
#include <QMap>
#include <QTransform>

// --- Enums ---

// Current tool for mouse interaction
enum class Tool {
    Select, // Select, move, resize, marquee select
    Draw,   // Draw new shapes
    Hand    // (Future) Pan the canvas
};

// Type of shape to draw
enum class ShapeType {
    Line,
    Rectangle,
    Circle
};

// All 8 sides where "drag handles" can be + none
enum class HandlePosition {
    None,
    TopLeft, Top, TopRight, // Top
    Left, Right, // Middle
    BottomLeft, Bottom, BottomRight, // Bottom
    Start, End // For lines
};

// --- Data Structure ---

struct Shape {
    ShapeType type;
    QRect rect;      // For shapes (Rectangle, Circle)
    QPoint start;    // For line
    QPoint end;      // For line
    bool selected = false;

    // Temp copy of stats of origin shape for resize
    QRect originalRect;
    QPoint originalStart;
    QPoint originalEnd;

    // Function to get selection border (bounding box)
    QRectF bounds() const {
        if (type == ShapeType::Line) {
            return QRectF(start, end).normalized();
        }
        return QRectF(rect);
    }
};

// --- Canvas Class ---

class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);

    // --- Public Setters (Slots) ---
public slots:
    void setShapeType(ShapeType type);
    void setTool(Tool tool);
    void setGridEnabled(bool enabled);
    void setSnapEnabled(bool enabled);

protected:
    // --- Qt Event Handlers ---
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // --- Grid & Snap Settings ---
    int gridSize = 20;
    bool gridEnabled = true;
    bool snapEnabled = true;

    // --- Current State ---
    Tool currentTool = Tool::Select;
    ShapeType currentShape = ShapeType::Line;

    // --- Action Flags (State Machine) ---
    bool drawing = false;   // True if drawing a new shape
    bool moving = false;    // True if moving selected shape(s)
    bool resizing = false;  // True if resizing a shape
    bool selecting = false; // True if drawing marquee selection rect

    // --- Action Geometry ---
    QPoint startPoint;      // Start point for 'drawing'
    QPoint lastMousePos;    // Last mouse pos for 'moving' delta
    QRect selectionRect;    // Geometry for 'selecting'

    // --- Core Data ---
    std::vector<Shape> shapes; // Array of all shapes

    // --- Resize Data ---
    Shape* resizingShape = nullptr; // Main resize shape
    HandlePosition currentResizeHandle = HandlePosition::None;
    std::vector<Shape> originalShapes; // Snapshot of all selected shapes

    // --- Private Helpers: Resize & Math ---
    void applyResize(const QPoint& mousePos, Qt::KeyboardModifiers modifiers);
    QPointF getAnchorPoint(const QRectF& rect, HandlePosition handle, bool fromCenter);
    QPointF scalePoint(const QPointF& p, const QPointF& anchor, qreal sx, qreal sy);
    QRect calculateRect(const QPoint& p1, const QPoint& p2) const;

    // --- Private Helpers: Hit-testing ---
    Shape* shapeAt(const QPoint &pos);
    std::pair<Shape*, HandlePosition> getHandleAt(const QPoint& pos);
    QMap<HandlePosition, QRectF> getResizeHandles(const Shape& s) const;

    // --- Private Helpers: UI & Grid ---
    void updateCursorIcon(const QPoint &pos = QPoint());
    void drawGrid(QPainter* p);
    QPoint snapToGrid(const QPoint& pos);
};

#endif // CANVAS_H
