#include "canvas.h"

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
}

void Canvas::setShapeType(ShapeType type) {
    currentShape = type;
}

Shape* Canvas::shapeAt(const QPoint &pos) {
    for (auto it = shapes.rbegin(); it != shapes.rend(); ++it) {
        Shape &s = *it;
        if (s.type == ShapeType::Line) {
            QLineF line(s.start, s.end);
            if (line.length() == 0) continue;

            QPointF p = pos;
            QPointF a = line.p1();
            QPointF b = line.p2();
            QPointF ab = b - a;
            QPointF ap = p - a;
            qreal t = QPointF::dotProduct(ap, ab) / QPointF::dotProduct(ab, ab);
            t = qBound(0.0, t, 1.0);
            QPointF closest = a + t * ab;
            qreal distance = QLineF(p, closest).length();
            if (distance < 5) return &s;
        } else {
            if (s.rect.contains(pos))
                return &s;
        }
    }
    return nullptr;
}

void Canvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    Shape* s = shapeAt(event->pos());

    if (s) {
        if (event->modifiers() & Qt::ShiftModifier) {
            s->selected = !s->selected;
        } else {
            // если клик на невыделенной фигуре — снимаем выделение с остальных
            if (!s->selected) {
                for (auto &shape : shapes) shape.selected = false;
                s->selected = true;
            }
        }

        // при клике на выделенной фигуре включаем режим перемещения
        if (s->selected) {
            moving = true;
            lastMousePos = event->pos();
        }

        selectedShape = s;
    } else {
        // клик вне фигур — снимаем выделение
        for (auto &shape : shapes) shape.selected = false;
        selectedShape = nullptr;

        if (currentTool == Tool::Draw) {
            drawing = true;
            startPoint = event->pos();
        }
    }

    update();
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
    if (drawing) {
        update();
        return;
    }

    if (moving) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();

        for (auto &s : shapes) {
            if (!s.selected) continue;

            if (s.type == ShapeType::Line) {
                s.start += delta;
                s.end += delta;
            } else {
                s.rect.translate(delta);
            }
        }
        update();
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (drawing) {
        if (currentShape == ShapeType::Line) {
            QLineF line(startPoint, event->pos());
            if (line.length() > 5)
                shapes.push_back(Shape{ShapeType::Line, QRect(), startPoint, event->pos(), false});
        } else {
            QRect r(startPoint, event->pos());
            if (abs(r.width()) > 5 && abs(r.height()) > 5)
                shapes.push_back(Shape{currentShape, r.normalized(), {}, {}, false});
        }
        drawing = false;
        update();
    }

    moving = false;
    selectedShape = nullptr;
}


void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setRenderHint(QPainter::Antialiasing);

    for (const auto &s : shapes) {
        // выделение
        if (s.selected) {
            QPen pen(Qt::blue, 2, Qt::DashLine);
            p.setPen(pen);
            if (s.type == ShapeType::Line) {
                QRectF lineRect(s.start, s.end);
                lineRect = lineRect.normalized().adjusted(-5, -5, 5, 5);
                p.drawRect(lineRect);
            } else {
                p.drawRect(s.rect.adjusted(-3, -3, 3, 3));
            }
        }

        // фигура
        QPen pen(Qt::black, 2);
        p.setPen(pen);
        switch (s.type) {
        case ShapeType::Line:
            p.drawLine(s.start, s.end);
            break;
        case ShapeType::Rectangle:
            p.drawRect(s.rect);
            break;
        case ShapeType::Circle:
            p.drawEllipse(s.rect);
            break;
        }
    }

    // предпросмотр рисования
    if (drawing) {
        p.setPen(Qt::gray);
        QPoint cur = mapFromGlobal(QCursor::pos());
        switch (currentShape) {
        case ShapeType::Line:
            p.drawLine(startPoint, cur);
            break;
        case ShapeType::Rectangle:
            p.drawRect(QRect(startPoint, cur));
            break;
        case ShapeType::Circle:
            p.drawEllipse(QRect(startPoint, cur));
            break;
        }
    }
}
