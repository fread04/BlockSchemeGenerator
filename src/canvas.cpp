#include "canvas.h"
#include <QApplication>
#include <algorithm>
#include <QDebug>
#include <QtMath> // Для qRound и qMax

// Глобальные константы
const int HANDLE_SIZE = 8;
const int CLICK_THRESHOLD = 5; // Порог "клика" (в пикселях)

//==================================================================
// 1. Public-функции (Конструктор и Сеттеры)
//==================================================================

/**
 * @brief Конструктор класса Canvas.
 */
Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setTool(Tool::Select); // Устанавливаем инструмент по умолчанию
}

/**
 * @brief Устанавливает текущий тип фигуры для рисования.
 */
void Canvas::setShapeType(ShapeType type) {
    currentShape = type;
}

/**
 * @brief Устанавливает текущий активный инструмент.
 */
void Canvas::setTool(Tool tool) {
    currentTool = tool;
    updateCursorIcon(); // Обновляем курсор в соответствии с инструментом
}

/**
 * @brief Включает или выключает отображение сетки.
 */
void Canvas::setGridEnabled(bool enabled) {
    gridEnabled = enabled;
    update(); // Запросить перерисовку, чтобы показать/скрыть сетку
}

/**
 * @brief Включает или выключает привязку к сетке.
 */
void Canvas::setSnapEnabled(bool enabled) {
    snapEnabled = enabled;
}

//==================================================================
// 2. Protected-функции (Главные обработчики событий)
//==================================================================

/**
 * @brief Главная функция отрисовки.
 */
void Canvas::paintEvent(QPaintEvent *e) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setRenderHint(QPainter::Antialiasing);

    // 0. РИСУЕМ СЕТКУ (самый нижний слой)
    if (gridEnabled) {
        drawGrid(&p);
    }

    // 1. РИСУЕМ ВСЕ ФИГУРЫ
    for (const auto &s : shapes) {
        QPen pen(Qt::black, 2); p.setPen(pen); p.setBrush(Qt::NoBrush);
        switch (s.type) {
        case ShapeType::Line: p.drawLine(s.start, s.end); break;
        case ShapeType::Rectangle: p.drawRect(s.rect); break;
        case ShapeType::Circle: p.drawEllipse(s.rect); break;
        }
    }

    // 2. РИСУЕМ ВЫДЕЛЕНИЕ И РУЧКИ
    // Рисуем выделение если: активен инструмент выделения, идет moving/resizing,
    // ИЛИ есть хотя бы одна выделенная фигура
    bool hasSelection = std::any_of(shapes.begin(), shapes.end(),
                                    [](const Shape& s) { return s.selected; });

    if (currentTool == Tool::Select || moving || resizing || hasSelection) {
        QPen selectionPen(Qt::blue, 1, Qt::DashLine);
        QBrush handleBrush(Qt::blue);
        for (const auto &s : shapes) {
            if (!s.selected) continue;
            QRectF b = s.bounds();

            p.setPen(selectionPen);
            p.setBrush(Qt::NoBrush);
            p.drawRect(b.adjusted(-3, -3, 3, 3)); // Рамка выделения

            p.setPen(Qt::NoPen);
            p.setBrush(handleBrush);
            auto handles = getResizeHandles(s);
            for (const QRectF& handleRect : handles.values()) {
                p.drawRect(handleRect); // Ручки ресайза
            }
        }
    }

    // 3. РИСУЕМ ПРЕДПРОСМОТР РИСОВАНИЯ
    if (drawing) {
        p.setPen(QPen(Qt::gray, 1, Qt::DashLine)); p.setBrush(Qt::NoBrush);

        QPoint snappedLastPos = snapToGrid(lastMousePos);

        // Используем хелпер для Shift/Ctrl
        QRect r = calculateRect(startPoint, snappedLastPos);

        if (currentShape == ShapeType::Line) {
            p.drawLine(startPoint, snappedLastPos);
        } else if (currentShape == ShapeType::Rectangle) {
            p.drawRect(r); // Рисуем прямоугольник с учетом Shift/Ctrl
        } else if (currentShape == ShapeType::Circle) {
            p.drawEllipse(r); // Рисуем эллипс/круг с учетом Shift/Ctrl
        }
    }

    // 4. РИСУЕМ ПРЯМОУГОЛЬНИК ВЫДЕЛЕНИЯ
    if (selecting) {
        p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        p.setBrush(QColor(0, 0, 255, 30));
        p.drawRect(selectionRect);
    }
}

/**
 * @brief Обрабатывает нажатие кнопки мыши.
 */
void Canvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    lastMousePos = event->pos(); // Сохраняем *реальную* позицию
    QPoint snappedPos = snapToGrid(event->pos()); // Используем *привязанную*

    if (currentTool == Tool::Select) {
        // Логика Tool::Select (без изменений)
        auto [handleShape, handlePos] = getHandleAt(event->pos());
        if (handleShape != nullptr) {
            resizing = true;
            resizingShape = handleShape;
            currentResizeHandle = handlePos;
            originalShapes.clear();
            for (auto& s : shapes) {
                if (s.selected) {
                    s.originalRect = s.rect;
                    s.originalStart = s.start;
                    s.originalEnd = s.end;
                    originalShapes.push_back(s);
                }
            }
            update();
            return;
        }

        Shape* s = shapeAt(event->pos());
        if (s) {
            moving = true;
            if (event->modifiers() & Qt::ShiftModifier) {
                s->selected = !s->selected;
            } else if (!s->selected) {
                for (auto &shape : shapes) shape.selected = false;
                s->selected = true;
            }
            update();
            return;
        }

        selecting = true;
        selectionRect = QRect(snappedPos, QSize(0, 0));
        if (!(event->modifiers() & Qt::ShiftModifier)) {
            for (auto &shape : shapes) shape.selected = false;
        }
        update();
        return;

    } else if (currentTool == Tool::Draw) {
        // СНАЧАЛА проверяем ручки ресайза
        auto [handleShape, handlePos] = getHandleAt(event->pos());
        if (handleShape != nullptr) {
            resizing = true;
            resizingShape = handleShape;
            currentResizeHandle = handlePos;
            originalShapes.clear();
            for (auto& s : shapes) {
                if (s.selected) {
                    s.originalRect = s.rect;
                    s.originalStart = s.start;
                    s.originalEnd = s.end;
                    originalShapes.push_back(s);
                }
            }
            update();
            return;
        }

        // Затем проверяем, не попали ли в фигуру
        Shape* s = shapeAt(event->pos());
        if (s) {
            // Попали в фигуру: выделяем ее (как Tool::Select)
            if (event->modifiers() & Qt::ShiftModifier) {
                s->selected = !s->selected;
            } else if (!s->selected) {
                for (auto &shape : shapes) shape.selected = false;
                s->selected = true;
            }

            // Явно сбрасываем drawing и включаем moving
            drawing = false;
            moving = true;

            update();
            return;

        } else {
            // Попали в пустое место: начинаем рисование
            drawing = true;
            moving = false;
            startPoint = snappedPos;
            for (auto &shape : shapes) shape.selected = false;
            update();
            return;
        }
    }
}

/**
 * @brief Обрабатывает движение мыши.
 */
void Canvas::mouseMoveEvent(QMouseEvent *event) {
    QPoint snappedPos = snapToGrid(event->pos());

    QPoint delta;
    if (moving || resizing) { // Перемещение и ресайз всегда привязаны
        delta = snappedPos - snapToGrid(lastMousePos);
    } else {
        delta = event->pos() - lastMousePos;
    }

    lastMousePos = event->pos();

    // 1. РЕСАЙЗ
    if (resizing) {
        applyResize(snappedPos, event->modifiers());
        return;
    }

    // 2. ПЕРЕМЕЩЕНИЕ
    if (moving) {
        if (delta.isNull()) return;

        for (auto &s : shapes) {
            if (!s.selected) continue;
            if (s.type == ShapeType::Line) {
                s.start += delta; s.end += delta;
            } else {
                s.rect.translate(delta);
            }
        }
        update();
        return;
    }

    // 3. ПРЯМОУГОЛЬНОЕ ВЫДЕЛЕНИЕ
    if (selecting) {
        selectionRect.setBottomRight(snappedPos); // Обновляем привязанным
        update();
        return;
    }

    // 4. РИСОВАНИЕ
    if (drawing) {
        update(); // Обновляем "призрачный" предпросмотр
        return;
    }

    // 5. Обновление курсора, если ничего не делаем
    updateCursorIcon(event->pos());
}

/**
 * @brief Обрабатывает отпускание кнопки мыши.
 */
void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    QPoint snappedPos = snapToGrid(event->pos());

    // 1. ЗАВЕРШЕНИЕ РЕСАЙЗА
    if (resizing) {
        resizing = false;
        resizingShape = nullptr;
        currentResizeHandle = HandlePosition::None;
        originalShapes.clear();
        update();
        updateCursorIcon(event->pos());
        return;
    }

    // 2. ЗАВЕРШЕНИЕ ПЕРЕМЕЩЕНИЯ
    // ВАЖНО: обрабатываем ПЕРЕД рисованием и выделением!
    if (moving) {
        moving = false;
        // НЕ сбрасываем выделение - фигура остается выделенной
        update();
        updateCursorIcon(event->pos());
        return;
    }

    // 3. ЗАВЕРШЕНИЕ ПРЯМОУГОЛЬНОГО ВЫДЕЛЕНИЯ
    if (selecting) {
        selecting = false;
        QRect selRect = selectionRect.normalized();
        for(auto& s : shapes) {
            if (selRect.contains(s.bounds().toRect())) {
                s.selected = true;
            }
        }
        update();
        updateCursorIcon(event->pos());
        return;
    }

    // 4. ЗАВЕРШЕНИЕ РИСОВАНИЯ
    if (drawing) {
        drawing = false;
        QPoint endPoint = snappedPos;

        int manhattan = (startPoint - endPoint).manhattanLength();
        bool isClick = (manhattan < CLICK_THRESHOLD);

        if (!isClick) {
            if (currentShape == ShapeType::Line) {
                shapes.push_back({currentShape, QRect(), startPoint, endPoint, true});
            } else {
                QRect r = calculateRect(startPoint, endPoint);
                shapes.push_back({currentShape, r, {}, {}, true});
            }

            // выделяем созданную фигуру
            for (auto &s : shapes) s.selected = false;
            if (!shapes.empty()) shapes.back().selected = true;
        }
        // Убрали обработку короткого клика - она не нужна, т.к. moving уже обработан выше

        update();
        updateCursorIcon(event->pos());
        return;
    }

    update();
    updateCursorIcon(event->pos());
}

/**
 * @brief Обрабатывает нажатие клавиш.
 */
void Canvas::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Удаляем все выделенные фигуры
        shapes.erase(
            std::remove_if(shapes.begin(), shapes.end(),
                           [](const Shape &s) { return s.selected; }),
            shapes.end());
        update();
    }
}

//==================================================================
// 3. Private-функции (Вспомогательные)
//==================================================================

// --- Логика Ресайза ---

/**
 * @brief Применяет логику ресайза ко всем выделенным фигурам.
 */
void Canvas::applyResize(const QPoint &mousePos, Qt::KeyboardModifiers modifiers) {
    bool keepProportions = modifiers & Qt::ShiftModifier; bool fromCenter = modifiers & Qt::ControlModifier;
    if (!resizingShape) return;
    auto primaryOrigIt = std::find_if(originalShapes.begin(), originalShapes.end(),
                                      [this](const Shape& s) { return s.originalRect == resizingShape->originalRect && s.originalStart == resizingShape->originalStart; });
    if (primaryOrigIt == originalShapes.end()) return;
    const Shape& primaryOriginal = *primaryOrigIt;
    qreal g_scaleX = 1.0, g_scaleY = 1.0; QPointF primaryAnchor; QPointF origHandlePos; QPointF origVector;
    if (primaryOriginal.type == ShapeType::Line) {
        origHandlePos = (currentResizeHandle == HandlePosition::Start) ? QPointF(primaryOriginal.originalStart) : QPointF(primaryOriginal.originalEnd);
        if (fromCenter) primaryAnchor = QLineF(primaryOriginal.originalStart, primaryOriginal.originalEnd).center();
        else primaryAnchor = (currentResizeHandle == HandlePosition::Start) ? QPointF(primaryOriginal.originalEnd) : QPointF(primaryOriginal.originalStart);
        origVector = origHandlePos - primaryAnchor;
    } else {
        QRectF origRect = primaryOriginal.bounds(); primaryAnchor = getAnchorPoint(origRect, currentResizeHandle, fromCenter);
        switch(currentResizeHandle) {
        case HandlePosition::TopLeft:       origHandlePos = origRect.topLeft(); break;
        case HandlePosition::Top:           origHandlePos = QPointF(origRect.center().x(), origRect.top()); break;
        case HandlePosition::TopRight:      origHandlePos = origRect.topRight(); break;
        case HandlePosition::Left:          origHandlePos = QPointF(origRect.left(), origRect.center().y()); break;
        case HandlePosition::Right:         origHandlePos = QPointF(origRect.right(), origRect.center().y()); break;
        case HandlePosition::BottomLeft:    origHandlePos = origRect.bottomLeft(); break;
        case HandlePosition::Bottom:        origHandlePos = QPointF(origRect.center().x(), origRect.bottom()); break;
        case HandlePosition::BottomRight:   origHandlePos = origRect.bottomRight(); break;
        default:                            origHandlePos = primaryAnchor;
        }
        origVector = origHandlePos - primaryAnchor;
    }
    QPointF newVector = mousePos - primaryAnchor;
    if (qAbs(origVector.x()) > 1e-3) g_scaleX = newVector.x() / origVector.x();
    if (qAbs(origVector.y()) > 1e-3) g_scaleY = newVector.y() / origVector.y();
    if (keepProportions) {
        qreal scale;
        if (primaryOriginal.type == ShapeType::Line) {
            qreal origLen = QLineF(QPointF(0,0), origVector).length(); qreal newLen = QLineF(QPointF(0,0), newVector).length();
            scale = (origLen == 0) ? 1.0 : (newLen / origLen);
        } else {
            if (currentResizeHandle == HandlePosition::Left || currentResizeHandle == HandlePosition::Right) scale = g_scaleX;
            else if (currentResizeHandle == HandlePosition::Top || currentResizeHandle == HandlePosition::Bottom) scale = g_scaleY;
            else scale = (qAbs(g_scaleX) > qAbs(g_scaleY)) ? g_scaleX : g_scaleY;
        }
        g_scaleX = scale; g_scaleY = scale;
    }
    if (primaryOriginal.type != ShapeType::Line && !keepProportions) {
        if (currentResizeHandle == HandlePosition::Top || currentResizeHandle == HandlePosition::Bottom) g_scaleX = 1.0;
        if (currentResizeHandle == HandlePosition::Left || currentResizeHandle == HandlePosition::Right) g_scaleY = 1.0;
    }
    if (primaryOriginal.type != ShapeType::Line && fromCenter && keepProportions) {
        if (currentResizeHandle == HandlePosition::Top || currentResizeHandle == HandlePosition::Bottom) g_scaleX = g_scaleY;
        if (currentResizeHandle == HandlePosition::Left || currentResizeHandle == HandlePosition::Right) g_scaleY = g_scaleX;
    }
    int orig_idx = 0;
    for (auto& s : shapes) {
        if (!s.selected) continue;
        const Shape& orig = originalShapes[orig_idx++]; QPointF s_anchor;
        if (fromCenter) {
            s_anchor = (orig.type == ShapeType::Line) ? QLineF(orig.originalStart, orig.originalEnd).center() : orig.bounds().center();
        } else {
            if (primaryOriginal.type == ShapeType::Line) {
                if (orig.type == ShapeType::Line) {
                    s_anchor = (currentResizeHandle == HandlePosition::Start) ? QPointF(orig.originalEnd) : QPointF(orig.originalStart);
                } else {
                    s_anchor = (currentResizeHandle == HandlePosition::Start) ? orig.bounds().bottomRight() : orig.bounds().topLeft();
                }
            } else {
                s_anchor = getAnchorPoint(orig.bounds(), currentResizeHandle, false);
            }
        }
        if (orig.type == ShapeType::Line) {
            s.start = scalePoint(orig.originalStart, s_anchor, g_scaleX, g_scaleY).toPoint();
            s.end = scalePoint(orig.originalEnd, s_anchor, g_scaleX, g_scaleY).toPoint();
        } else {
            QPointF newTopLeft = scalePoint(orig.rect.topLeft(), s_anchor, g_scaleX, g_scaleY);
            QPointF newBottomRight = scalePoint(orig.rect.bottomRight(), s_anchor, g_scaleX, g_scaleY);
            s.rect = QRectF(newTopLeft, newBottomRight).normalized().toRect();
        }
    }
    update();
}

/**
 * @brief Вычисляет "якорную" точку для ресайза.
 */
QPointF Canvas::getAnchorPoint(const QRectF& rect, HandlePosition handle, bool fromCenter) {
    if (fromCenter) return rect.center();
    switch (handle) {
    case HandlePosition::TopLeft:       return rect.bottomRight();
    case HandlePosition::Top:           return QPointF(rect.center().x(), rect.bottom());
    case HandlePosition::TopRight:      return rect.bottomLeft();
    case HandlePosition::Left:          return QPointF(rect.right(), rect.center().y());
    case HandlePosition::Right:         return QPointF(rect.left(), rect.center().y());
    case HandlePosition::BottomLeft:    return rect.topRight();
    case HandlePosition::Bottom:        return QPointF(rect.center().x(), rect.top());
    case HandlePosition::BottomRight:   return rect.topLeft();
    default:                            return rect.center();
    }
}

/**
 * @brief Масштабирует точку 'p' относительно якоря 'anchor'.
 */
QPointF Canvas::scalePoint(const QPointF &p, const QPointF &anchor, qreal sx, qreal sy) {
    return QPointF(
        anchor.x() + (p.x() - anchor.x()) * sx,
        anchor.y() + (p.y() - anchor.y()) * sy
        );
}

/**
 * @brief Вычисляет геометрию QRect на основе двух точек и модификаторов.
 *
 * Учитывает Shift (для квадратов) и Ctrl (для рисования от центра).
 *
 * @param p1 Первая точка (обычно startPoint).
 * @param p2 Вторая точка (обычно позиция мыши).
 * @return QRect Вычисленный прямоугольник.
 */
QRect Canvas::calculateRect(const QPoint& p1, const QPoint& p2) const {
    bool shift = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    bool ctrl = QApplication::keyboardModifiers() & Qt::ControlModifier;

    QRect r;

    if (ctrl) {
        // Ctrl: Рисование от центра (p1 - центр)
        int w = qAbs(p2.x() - p1.x()) * 2;
        int h = qAbs(p2.y() - p1.y()) * 2;
        if (shift) {
            w = h = qMax(w, h); // Ctrl + Shift = квадрат от центра
        }
        r = QRect(p1.x() - w/2, p1.y() - h/2, w, h);
    } else if (shift) {
        // Shift: Квадрат (p1 - угол)
        int w = p2.x() - p1.x();
        int h = p2.y() - p1.y();
        int size = qMax(qAbs(w), qAbs(h));

        // Сохраняем направление (квадрант)
        r = QRect(p1.x(), p1.y(),
                  (w < 0) ? -size : size,
                  (h < 0) ? -size : size);
    } else {
        // Свободное рисование
        r = QRect(p1, p2);
    }

    return r.normalized(); // Всегда возвращаем L-T < R-B
}

// --- Логика Определения (Hit-testing) ---

/**
 * @brief Находит фигуру в указанной позиции.
 */
Shape* Canvas::shapeAt(const QPoint &pos) {
    // Ищем от конца к началу, чтобы приоритет был у верхних фигур
    for (auto it = shapes.rbegin(); it != shapes.rend(); ++it) {
        Shape &s = *it;
        if (s.type == ShapeType::Line) {
            // Проверка для линии: ищем ближайшую точку на отрезке
            QLineF line(s.start, s.end);
            if (line.length() == 0) continue;

            QPointF p = pos; QPointF a = line.p1(); QPointF b = line.p2(); QPointF ab = b - a;
            QPointF ap = p - a; qreal t = QPointF::dotProduct(ap, ab) / QPointF::dotProduct(ab, ab);

            t = qBound(0.0, t, 1.0); // Ограничиваем t от 0 до 1 (точка должна быть на отрезке)
            QPointF closest = a + t * ab;

            qreal distance = QLineF(p, closest).length();
            if (distance < 5) return &s; // Порог 5 пикселей

        } else {
            // Проверка для прямоугольника/круга: попадание в область
            // Даем небольшой отступ (-2, 2) для удобства
            if (s.rect.adjusted(-2, -2, 2, 2).contains(pos)) return &s;
        }
    }
    return nullptr;
}

/**
 * @brief Находит ручку ресайза в указанной позиции.
 */
std::pair<Shape*, HandlePosition> Canvas::getHandleAt(const QPoint &pos) {
    // if (currentTool != Tool::Select) {
    //     return {nullptr, HandlePosition::None};
    // }
    for (auto& s : shapes) {
        if (!s.selected) continue;
        auto handles = getResizeHandles(s);
        for (auto it = handles.constBegin(); it != handles.constEnd(); ++it) {
            if (it.value().contains(pos)) {
                return {&s, it.key()};
            }
        }
    }
    return {nullptr, HandlePosition::None};
}

/**
 * @brief Вычисляет геометрию ручек ресайза для фигуры.
 */
QMap<HandlePosition, QRectF> Canvas::getResizeHandles(const Shape &s) const {
    QMap<HandlePosition, QRectF> handles; qreal h = HANDLE_SIZE; qreal h2 = h / 2.0;
    if (s.type == ShapeType::Line) {
        handles[HandlePosition::Start] = QRectF(s.start.x() - h2, s.start.y() - h2, h, h);
        handles[HandlePosition::End] = QRectF(s.end.x() - h2, s.end.y() - h2, h, h);
    } else {
        QRectF r = s.bounds();
        handles[HandlePosition::TopLeft] = QRectF(r.topLeft().x() - h2, r.topLeft().y() - h2, h, h);
        handles[HandlePosition::Top] = QRectF(r.center().x() - h2, r.top() - h2, h, h);
        handles[HandlePosition::TopRight] = QRectF(r.topRight().x() - h2, r.topRight().y() - h2, h, h);
        handles[HandlePosition::Left] = QRectF(r.left() - h2, r.center().y() - h2, h, h);
        handles[HandlePosition::Right] = QRectF(r.right() - h2, r.center().y() - h2, h, h);
        handles[HandlePosition::BottomLeft] = QRectF(r.bottomLeft().x() - h2, r.bottomLeft().y() - h2, h, h);
        handles[HandlePosition::Bottom] = QRectF(r.center().x() - h2, r.bottom() - h2, h, h);
        handles[HandlePosition::BottomRight] = QRectF(r.bottomRight().x() - h2, r.bottomRight().y() - h2, h, h);
    }
    return handles;
}

// --- Логика UI ---

/**
 * @brief Обновляет иконку курсора.
 */
void Canvas::updateCursorIcon(const QPoint &pos) {
    // Сначала проверяем режим перемещения
    if (moving) {
        setCursor(Qt::SizeAllCursor);
        return;
    }

    // Проверяем ручки ресайза (независимо от инструмента)
    auto [handleShape, handlePos] = getHandleAt(pos);
    if (handleShape) {
        switch (handlePos) {
        case HandlePosition::TopLeft: case HandlePosition::BottomRight:
        case HandlePosition::Start: case HandlePosition::End:
            setCursor(Qt::SizeFDiagCursor); break;
        case HandlePosition::TopRight: case HandlePosition::BottomLeft:
            setCursor(Qt::SizeBDiagCursor); break;
        case HandlePosition::Top: case HandlePosition::Bottom:
            setCursor(Qt::SizeVerCursor); break;
        case HandlePosition::Left: case HandlePosition::Right:
            setCursor(Qt::SizeHorCursor); break;
        default: setCursor(Qt::ArrowCursor); break;
        }
        return;
    }

    // Проверяем попадание на фигуру
    if (shapeAt(pos)) {
        setCursor(Qt::SizeAllCursor);
        return;
    }

    // По умолчанию ставим курсор в зависимости от инструмента
    if (currentTool == Tool::Draw) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

/**
 * @brief Рисует фон сетки (линиями).
 */
void Canvas::drawGrid(QPainter* p) {
    if (gridSize <= 0) return;

    // Сетка стала бледнее
    QPen pen(QColor(240, 240, 240), 1, Qt::SolidLine); // Используем сплошную линию
    p->setPen(pen);

    int w = width();
    int h = height();

    for (int x = 0; x < w; x += gridSize) {
        p->drawLine(x, 0, x, h);
    }
    for (int y = 0; y < h; y += gridSize) {
        p->drawLine(0, y, w, y);
    }
}

/**
 * @brief Привязывает точку к ближайшему узлу сетки.
 */
QPoint Canvas::snapToGrid(const QPoint& pos) {
    if (!snapEnabled || gridSize <= 0) {
        return pos; // Привязка выключена, возвращаем как есть
    }

    // qRound математически округляет к ближайшему целому
    int snappedX = qRound(pos.x() / (double)gridSize) * gridSize;
    int snappedY = qRound(pos.y() / (double)gridSize) * gridSize;

    return QPoint(snappedX, snappedY);
}
