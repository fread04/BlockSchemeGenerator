#include "canvas.h"
#include <QApplication>
#include <algorithm>
#include <QDebug>

// Глобальные константы
const int HANDLE_SIZE = 8;
const int CLICK_THRESHOLD = 5; // Порог "клика" (в пикселях)

//==================================================================
// 1. Public-функции (Конструктор и Сеттеры)
//==================================================================

/**
 * @brief Конструктор класса Canvas.
 *
 * Инициализирует виджет, устанавливает отслеживание мыши
 * и политику фокуса для обработки событий.
 *
 * @param parent Родительский виджет (обычно nullptr).
 */
Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setTool(Tool::Select); // Устанавливаем инструмент по умолчанию
}

//==================================================================
// 2. Protected-функции (Главные обработчики событий)
//==================================================================

/**
 * @brief Главная функция отрисовки.
 *
 * Вызывается Qt, когда виджету требуется перерисовка (по update()).
 * Рисует все фигуры, рамки выделения, ручки ресайза
 * и "призрачный" предпросмотр.
 *
 * @param e Событие отрисовки (не используется).
 */
void Canvas::paintEvent(QPaintEvent *e) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setRenderHint(QPainter::Antialiasing);

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
    if (currentTool == Tool::Select) {
        QPen selectionPen(Qt::blue, 1, Qt::DashLine);
        QBrush handleBrush(Qt::blue);
        for (const auto &s : shapes) {
            if (!s.selected) continue;
            p.setPen(selectionPen); p.setBrush(Qt::NoBrush);
            QRectF b = s.bounds();
            p.drawRect(b.adjusted(-3, -3, 3, 3));

            p.setPen(Qt::NoPen); p.setBrush(handleBrush);
            auto handles = getResizeHandles(s);
            for (const QRectF& handleRect : handles.values()) {
                p.drawRect(handleRect);
            }
        }
    }

    // 3. РИСУЕМ ПРЕДПРОСМОТР РИСОВАНИЯ
    if (drawing) {
        p.setPen(QPen(Qt::gray, 1, Qt::DashLine)); p.setBrush(Qt::NoBrush);
        QRect r(startPoint, lastMousePos);
        switch (currentShape) {
        case ShapeType::Line: p.drawLine(startPoint, lastMousePos); break;
        case ShapeType::Rectangle: p.drawRect(r); break;
        case ShapeType::Circle: p.drawEllipse(r); break;
        }
    }

    // 4. РИСУЕМ ПРЯМОУГОЛЬНИК ВЫДЕЛЕНИЯ
    if (selecting) {
        p.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        p.setBrush(QColor(0, 0, 255, 30)); // Полупрозрачный синий
        p.drawRect(selectionRect);
    }
}

/**
 * @brief Обрабатывает нажатие кнопки мыши.
 *
 * "Главный диспетчер" - решает, какое действие (рисование,
 * перемещение, ресайз, выделение) начинается.
 *
 * @param event Событие мыши, содержит позицию и кнопки.
 */
void Canvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    lastMousePos = event->pos();

    if (currentTool == Tool::Select) {
        // Приоритет 1: Нажали на ручку ресайза?
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

        // Приоритет 2: Нажали на фигуру? (Перемещение / Выделение)
        Shape* s = shapeAt(event->pos());
        if (s) {
            moving = true;
            if (event->modifiers() & Qt::ShiftModifier) {
                s->selected = !s->selected; // Инвертируем
            } else if (!s->selected) {
                for (auto &shape : shapes) shape.selected = false;
                s->selected = true;
            }
            update();
            return;
        }

        // Приоритет 3: Нажали на пустое место? (Прямоугольное выделение)
        selecting = true;
        selectionRect = QRect(event->pos(), QSize(0, 0));
        // Сбрасываем выделение, если только не зажат Shift
        if (!(event->modifiers() & Qt::ShiftModifier)) {
            for (auto &shape : shapes) shape.selected = false;
        }
        update();
        return;

    } else if (currentTool == Tool::Draw) {
        // Приоритет 1: Рисование
        drawing = true;
        startPoint = event->pos();
        // Сбрасываем выделение
        for (auto &shape : shapes) shape.selected = false;
        update();
        return;
    }
}

/**
 * @brief Обрабатывает движение мыши.
 *
 * Вызывается, когда мышь движется. Обновляет состояние
 * в зависимости от текущего действия (resizing, moving, etc.)
 *
 * @param event Событие мыши.
 */
void Canvas::mouseMoveEvent(QMouseEvent *event) {
    // Рассчитываем смещение с прошлого кадра
    QPoint delta = event->pos() - lastMousePos;
    lastMousePos = event->pos();

    // 1. РЕСАЙЗ
    if (resizing) {
        applyResize(event->pos(), event->modifiers());
        return;
    }

    // 2. ПЕРЕМЕЩЕНИЕ
    if (moving) {
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
        // Обновляем геометрию прямоугольника выделения
        selectionRect.setBottomRight(event->pos());
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
 *
 * Завершает текущее действие (resizing, moving, drawing, selecting).
 *
 * @param event Событие мыши.
 */
void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    // 1. ЗАВЕРШЕНИЕ РЕСАЙЗА
    if (resizing) {
        resizing = false;
        resizingShape = nullptr;
        currentResizeHandle = HandlePosition::None;
        originalShapes.clear();
    }

    // 2. ЗАВЕРШЕНИЕ ПРЯМОУГОЛЬНОГО ВЫДЕЛЕНИЯ
    if (selecting) {
        selecting = false;
        QRect selRect = selectionRect.normalized(); // "Нормализуем" прямоугольник

        // Выделяем все фигуры, чьи *габариты* полностью
        // попадают внутрь прямоугольника выделения.
        for(auto& s : shapes) {
            if (selRect.contains(s.bounds().toRect())) {
                s.selected = true;
            }
        }
    }

    // 3. ЗАВЕРШЕНИЕ РИСОВАНИЯ
    if (drawing) {
        drawing = false;

        int manhattan = (startPoint - event->pos()).manhattanLength();
        bool isClick = (manhattan <= CLICK_THRESHOLD);

        if (!isClick) {
            // Это был "дрэг" - создаем фигуру
            if (currentShape == ShapeType::Line) {
                shapes.push_back({currentShape, QRect(), startPoint, event->pos(), true});
            } else {
                QRect r(startPoint, event->pos());
                shapes.push_back({currentShape, r.normalized(), {}, {}, true});
            }
            // Выделяем только что созданную фигуру
            if (!shapes.empty()) shapes.back().selected = true;
        }
        // Если был "клик" (isClick == true), ничего не делаем.
    }

    // 4. ЗАВЕРШЕНИЕ ПЕРЕМЕЩЕНИЯ
    if (moving) {
        moving = false;
    }

    update();
    updateCursorIcon(event->pos());
}

/**
 * @brief Обрабатывает нажатие клавиш.
 *
 * Используется для удаления выделенных фигур.
 *
 * @param event Событие клавиатуры.
 */
void Canvas::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Идиома erase-remove для удаления всех s.selected == true
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
 *
 * Это "мозг" ресайза. Вычисляет коэффициенты масштабирования
 * на основе "главной" фигуры (resizingShape) и применяет их
 * ко всем остальным выделенным фигурам пропорционально.
 *
 * @param mousePos Текущая позиция мыши.
 * @param modifiers Зажатые клавиши (Shift, Ctrl).
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
        case HandlePosition::TopLeft:     origHandlePos = origRect.topLeft(); break;
        case HandlePosition::Top:         origHandlePos = QPointF(origRect.center().x(), origRect.top()); break;
        case HandlePosition::TopRight:    origHandlePos = origRect.topRight(); break;
        case HandlePosition::Left:        origHandlePos = QPointF(origRect.left(), origRect.center().y()); break;
        case HandlePosition::Right:       origHandlePos = QPointF(origRect.right(), origRect.center().y()); break;
        case HandlePosition::BottomLeft:  origHandlePos = origRect.bottomLeft(); break;
        case HandlePosition::Bottom:      origHandlePos = QPointF(origRect.center().x(), origRect.bottom()); break;
        case HandlePosition::BottomRight: origHandlePos = origRect.bottomRight(); break;
        default:                          origHandlePos = primaryAnchor;
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
 *
 * Якорь - это неподвижная точка при ресайзе.
 * (Напр., при Ctrl - центр, при тяге за TopLeft - BottomRight).
 *
 * @param rect Габариты фигуры.
 * @param handle Ручка, за которую тянут.
 * @param fromCenter Зажат ли Ctrl (масштабировать от центра).
 * @return QPointF Координата якорной точки.
 */
QPointF Canvas::getAnchorPoint(const QRectF& rect, HandlePosition handle, bool fromCenter) {
    if (fromCenter) return rect.center();
    switch (handle) {
    case HandlePosition::TopLeft:     return rect.bottomRight();
    case HandlePosition::Top:         return QPointF(rect.center().x(), rect.bottom());
    case HandlePosition::TopRight:    return rect.bottomLeft();
    case HandlePosition::Left:        return QPointF(rect.right(), rect.center().y());
    case HandlePosition::Right:       return QPointF(rect.left(), rect.center().y());
    case HandlePosition::BottomLeft:  return rect.topRight();
    case HandlePosition::Bottom:      return QPointF(rect.center().x(), rect.top());
    case HandlePosition::BottomRight: return rect.topLeft();
    default:                          return rect.center();
    }
}

/**
 * @brief Масштабирует точку 'p' относительно якоря 'anchor'.
 *
 * @param p Точка для масштабирования.
 * @param anchor Якорная точка (центр трансформации).
 * @param sx Коэффициент масштабирования по X.
 * @param sy Коэффициент масштабирования по Y.
 * @return QPointF Новая, масштабированная точка.
 */
QPointF Canvas::scalePoint(const QPointF &p, const QPointF &anchor, qreal sx, qreal sy) {
    return QPointF(
        anchor.x() + (p.x() - anchor.x()) * sx,
        anchor.y() + (p.y() - anchor.y()) * sy
        );
}

// --- Логика Определения (Hit-testing) ---

/**
 * @brief Находит фигуру в указанной позиции.
 *
 * Ищет в обратном порядке (сверху вниз).
 *
 * @param pos Позиция курсора мыши.
 * @return Shape* Указатель на найденную фигуру или nullptr.
 */
Shape* Canvas::shapeAt(const QPoint &pos) {
    for (auto it = shapes.rbegin(); it != shapes.rend(); ++it) {
        Shape &s = *it;
        if (s.type == ShapeType::Line) {
            QLineF line(s.start, s.end);
            if (line.length() == 0) continue;
            QPointF p = pos; QPointF a = line.p1(); QPointF b = line.p2(); QPointF ab = b - a;
            QPointF ap = p - a; qreal t = QPointF::dotProduct(ap, ab) / QPointF::dotProduct(ab, ab);
            t = qBound(0.0, t, 1.0); QPointF closest = a + t * ab;
            qreal distance = QLineF(p, closest).length();
            if (distance < 5) return &s;
        } else {
            if (s.rect.adjusted(-2, -2, 2, 2).contains(pos)) return &s;
        }
    }
    return nullptr;
}

/**
 * @brief Находит ручку ресайза в указанной позиции.
 *
 * Ищет ручки только у выделенных фигур.
 *
 * @param pos Позиция курсора мыши.
 * @return std::pair<Shape*, HandlePosition> Пара (Указатель на фигуру, Тип ручки) или {nullptr, None}.
 */
std::pair<Shape*, HandlePosition> Canvas::getHandleAt(const QPoint &pos) {
    if (currentTool != Tool::Select) {
        return {nullptr, HandlePosition::None};
    }
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
 *
 * @param s Фигура, для которой нужны ручки.
 * @return QMap<HandlePosition, QRectF> Словарь (Тип ручки -> Геометрия ручки).
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
 *
 * Меняет курсор в зависимости от инструмента и того,
 * над чем находится мышь (ручка, фигура, пустое место).
 *
 * @param pos Текущая позиция мыши.
 */
void Canvas::updateCursorIcon(const QPoint &pos) {
    if (currentTool == Tool::Draw) {
        setCursor(Qt::CrossCursor);
        return;
    }

    // (currentTool == Tool::Select)
    if (moving) {
        setCursor(Qt::SizeAllCursor);
        return;
    }

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
    } else if (shapeAt(pos)) {
        setCursor(Qt::SizeAllCursor);
    } else {
        // В режиме Select над пустым местом - обычная стрелка
        setCursor(Qt::ArrowCursor);
    }
}
