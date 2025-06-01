#include <QGuiApplication>
#include <QObject>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <map>

#include "Waveform.hpp"

enum Layers
{
   Layers_ScaleLine = 1,
   Layers_Graphics,
   Layers_Cursor
};

WaveformView::WaveformView(QWidget* parent)
   : QGraphicsView(parent)
   , m_cursorLine(nullptr)
{
   setRenderHint(QPainter::Antialiasing);
   setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   setAlignment(Qt::AlignLeft | Qt::AlignTop);

   // Создаем основную сцену
   m_scene = new QGraphicsScene(this);
   m_scene->setBackgroundBrush(Qt::black);
   setScene(m_scene);

   // Создаем отдельную сцену для временной шкалы
   m_scaleScene = new QGraphicsScene(this);

   // Создаем отдельный QGraphicsView для временной шкалы
   m_scaleView = new QGraphicsView(m_scaleScene);
   m_scaleView->setFixedHeight(SCALE_LINE_HEIGHT); // Высота временной шкалы
   m_scaleView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   m_scaleView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   m_scaleView->setRenderHint(QPainter::Antialiasing);
   m_scaleView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
   m_scaleScene->setBackgroundBrush(Qt::black);

   m_scaleView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
   m_scaleView->setScene(m_scaleScene);

   auto linePen = QPen(Qt::red, 1, Qt::SolidLine);
   linePen.setCosmetic(true);
   m_cursorLine = m_scene->addLine(0, 0, 0, 0, linePen);

   connect(horizontalScrollBar(), &QScrollBar::valueChanged,
           m_scaleView->horizontalScrollBar(), &QScrollBar::setValue);

   connect(m_scaleView->horizontalScrollBar(), &QScrollBar::valueChanged,
           horizontalScrollBar(), &QScrollBar::setValue);

   connect(m_scaleView->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]()
   {
      // UpdateSignals(m_signals);
      DrawScaleLine();
   });

   connect(m_scaleView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]()
   {
      // UpdateSignals(m_signals);
      DrawScaleLine();
   });

   connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &WaveformView::UpdateScaleViewWidth);

   auto* sb = new SnapScrollBar(Qt::Vertical, this);
   sb->setSnapStep(WAVEFORM_HEIGHT + SPACING);
   setVerticalScrollBar(sb);

   UpdateScaleViewWidth(); // начальная настройка
}

void
WaveformView::mousePressEvent(QMouseEvent* event)
{
   if (event->button() == Qt::LeftButton)
   {
      QPointF scenePos = mapToScene(event->pos());
      // Перемещаем курсор
      if (m_cursorLine)
      {
         std::size_t height = m_signals.size() * (WAVEFORM_HEIGHT + SPACING);
         height = height > this->height() ? height : this->height();
         m_cursorLine->setLine(scenePos.x(), 0, scenePos.x(), height);
         m_cursorLine->setZValue(Layers_Cursor);
         m_cursorPos = scenePos.x();

         emit SelectedTimestampChange(scenePos.x());
      }
   }
   QGraphicsView::mousePressEvent(event);
}

void
WaveformView::wheelEvent(QWheelEvent* event)
{
   int delta = event->angleDelta().y(); // Получаем направление прокрутки (вертикальное колесо мыши)

   // Прокрутка горизонтального скроллбара
   if (horizontalScrollBar()->isVisible())
   {
      QScrollBar* hScrollBar = horizontalScrollBar();
      int currentValue = hScrollBar->value();
      int step = 15; // Размер шага прокрутки
      int newValue = currentValue - delta / 120 * step;

      // Убедимся, что значение в пределах допустимых границ
      newValue = std::clamp(newValue, hScrollBar->minimum(), hScrollBar->maximum());

      // Устанавливаем новое значение
      hScrollBar->setValue(newValue);
   }

   event->accept(); // Событие обработано
}

void
WaveformView::resizeEvent(QResizeEvent* event)
{
   QGraphicsView::resizeEvent(event);
   UpdateScaleViewWidth();
   UpdateSignals(m_signals);
}

void
WaveformView::ZoomIn()
{
   ZoomX(m_currentZoomLevel + 1);
}

bool
WaveformView::ZoomOut()
{
   if (!HasScrollBar())
   {
      return false;
   }
   ZoomX(m_currentZoomLevel - 1);
   DrawScaleLine();
   return true;
}

void
WaveformView::SetHandle(
   std::shared_ptr<newVcd::Handle> newHandle)
{
   m_handle = newHandle;

   m_scene->clear();        ///< Сцена с диаграммами сигналов.
   m_scaleScene->clear();   ///< Сцена с временной шкалой.
   m_signals.clear();       ///< Отображаемые сигналы.
   m_currentZoomLevel = 0;  ///< Текущий уровень масштабирования.
   m_minZoomLevel = 0;      ///< Минимальный оптимальный уровень зума
   m_cursorPos = 0;         ///< Позиция курсора по оси X.
   m_currentScaleValue = 0; ///< Текущий масштаб по оси X.
   m_expandedMap.clear();   ///< Отслеживание раскрытия многоразрядных сигналов.
   m_aliasItemMap.clear();

   m_scaleView->resetTransform();
   resetTransform();
   auto linePen = QPen(Qt::red, 1, Qt::SolidLine);
   linePen.setCosmetic(true);
   m_cursorLine = m_scene->addLine(0, 0, 0, 0, linePen);

   if (!m_handle)
   {
      return;
   }
   m_handle->LoadSignals();

   DrawScaleLine(true);
   m_scene->setSceneRect(0, 0, newHandle->maxTs(), height());
}

void
WaveformView::OnItemExpandedOrCollapsed(newVcd::PinDescriptionPtr pin, bool isExpanded)
{
   // m_expandedMap[pin] = isExpanded;
   //((MultipleWaveItem*)m_aliasItemMap[pin->alias()])->SetExpanded(isExpanded);
   //
   // int yOff = 0;
   // for (const auto& it : m_aliasOrder)
   //{
   //   QGraphicsItem* item = m_aliasItemMap[it];
   //   item->setPos(0, yOff);
   //   yOff += item->boundingRect().height();
   //}
   // UpdateSignals(m_signals);
}

void
WaveformView::UpdateSignals(std::vector<newVcd::PinDescriptionPtr> newSignals)
{
   if (!m_handle)
      return;

   /* --- удалить исчезнувшие --- */
   for (auto it = m_aliasItemMap.begin(); it != m_aliasItemMap.end();)
   {
      if (std::none_of(newSignals.begin(), newSignals.end(),
                       [&](auto& p)
      { return p->alias() == it->first; }))
      {
         m_scene->removeItem(it->second);
         delete it->second;
         it = m_aliasItemMap.erase(it);
      }
      else
      {
         ++it;
      }
   }

   m_aliasOrder.clear();
   // m_aliasOrder.reserve(200);
   /* --- добавить / reposition --- */
   int yOff = 0;
   for (auto pin : newSignals)
   {

      if (!m_aliasItemMap.count(std::string(pin->alias())))
      {
         QGraphicsItem* item = nullptr;

         if (pin->pinType() == newVcd::PinType::parameter)
         {
            item = new ParamWaveItem(m_handle, std::static_pointer_cast<newVcd::ParamPinDescription>(pin), yOff);
         }
         else if (pin->sigType() == newVcd::SigType::simple)
         {
            item = new SimpleWaveItem(m_handle, std::static_pointer_cast<newVcd::SimplePinDescription>(pin), yOff);
         }
         else
         {
            // item = new MultipleWaveItem(m_handle, pin, yOff);
            // connect(this, &WaveformView::ScaleCoeffChanged, (MultipleWaveItem*)item, &MultipleWaveItem::SetScaleCoeff);
         }
         m_scene->addItem(item);
         m_aliasItemMap[std::string(pin->alias())] = item;
         yOff += item->boundingRect().height();
         m_aliasOrder.push_back(std::string(pin->alias()));
      }
      else
      {
         m_aliasItemMap[std::string(pin->alias())]->setPos(0, yOff);
         yOff += m_aliasItemMap[std::string(pin->alias())]->boundingRect().height();
         m_aliasOrder.push_back(std::string(pin->alias()));
      }
   }
   m_signals = std::move(newSignals);

   /* высота сцены */
   m_scene->setSceneRect(0, 0,
                         m_handle->maxTs(),
                         std::max(yOff, height()));

   DrawScaleLine();

   std::size_t height = m_signals.size() * (WAVEFORM_HEIGHT + SPACING);
   height = height > this->height() ? height : this->height();
   m_cursorLine->setLine(m_cursorPos, 0, m_cursorPos, height);
}

void
WaveformView::UpdateScaleViewWidth()
{
   int scrollbarWidth = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
   m_scaleView->setFixedWidth(viewport()->width());
}

QGraphicsView*
WaveformView::GetScaleView()
{
   return m_scaleView;
}

QScrollBar*
WaveformView::GetVerticalScrollBar() const
{
   return verticalScrollBar();
}

qreal
WaveformView::CalculateDevicePixelRatio() const
{
   QScreen* screen = nullptr;

   if (windowHandle())
   {
      screen = windowHandle()->screen();
   }

   if (!screen)
   {
      screen = QGuiApplication::primaryScreen();
   }

   if (screen)
   {
      qreal dpr = screen->devicePixelRatio();
      return dpr;
   }

   return 1.0;
}

void
WaveformView::SetInitialScale()
{
   if (!m_scaleView || !m_handle)
   {
      return;
   }
   QRectF viewRect = m_scaleView->viewport()->rect();

   qreal scaleX = (m_dpr * viewRect.width()) / static_cast<qreal>(m_handle->maxTs());
   scale(scaleX, 1.0);
   m_scaleView->scale(scaleX, 1.0);

   while (ZoomOut())
   {
   }
}

void
WaveformView::ZoomX(int level)
{
   qreal scaleFactor = 1.0 + (level - m_currentZoomLevel) * 0.1;
   scale(scaleFactor, 1.0); // Масштабируем только по оси X
   m_scaleView->scale(scaleFactor, 1.0);
   m_currentZoomLevel = level;
   // UpdateSignals(m_signals);
   emit ScaleCoeffChanged(CalcScaleCoeff());
}

double
WaveformView::CalcScaleCoeff() const
{
   QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();

   // Calculate scale coefficient
   const double scaleCoeff = visibleRect.width() / width();
   return scaleCoeff;
}

void
WaveformView::DrawScaleLine(bool reset)
{
   if (!m_handle)
   {
      return;
   }

   double visibleStart = 0;
   double visibleEnd = 0;
   double visibleWidth = 0;
   // Получаем видимую область
   if (reset)
   {
      visibleStart = 0;
      visibleEnd = m_handle->maxTs();
      if (visibleEnd == 0)
      {
         return;
      }
      visibleWidth = visibleEnd;
   }
   else
   {
      QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();
      visibleStart = visibleRect.left();
      visibleEnd = visibleRect.right();
      visibleWidth = visibleRect.width();
   }

   m_scaleScene->clear();
   m_scaleScene->setSceneRect(0, -SCALE_LINE_HEIGHT, m_handle->maxTs(), SCALE_LINE_HEIGHT);

   // Определяем шаги для различных единиц измерения
   static const std::unordered_map<std::string, double> scaleMap = {
      {"ps", 1.0}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}};
   static const std::vector<std::string> units = {"ps", "ns", "us", "ms", "s"};

   auto iterFirstNotNumber = std::find_if(m_handle->timeScale().begin(), m_handle->timeScale().end(), [](char c)
   {
      return !std::isdigit(static_cast<unsigned char>(c));
   });

   std::size_t mulFactor = std::stol(std::string(m_handle->timeScale().substr(0, std::distance(m_handle->timeScale().begin(), iterFirstNotNumber))));
   std::string currentUnit = std::string(m_handle->timeScale().substr(std::distance(m_handle->timeScale().begin(), iterFirstNotNumber)));
   double timescaleFactor = scaleMap.at(currentUnit);
   int unitIndex = std::distance(units.begin(), std::find(units.begin(), units.end(), currentUnit));

   // Начальный расчет шага деления для оптимальной видимости
   double optimalStep = visibleWidth / 10.0; // Стремимся к 10 основным делениям в видимой области
   double increment = optimalStep * timescaleFactor;

   // Переход на более крупную единицу, если шаг слишком мал
   while (unitIndex < units.size() - 1 && increment >= 1000)
   {
      increment /= 1000;
      timescaleFactor /= 1000;
      ++unitIndex;
   }
   currentUnit = units[unitIndex];

   // Приводим increment к «круглому» числу для крупных делений (например, 10, 50, 100)
   double base = std::pow(10, std::floor(std::log10(increment)));
   if (increment / base >= 5)
      increment = 5 * base;
   else if (increment / base >= 2)
      increment = 2 * base;
   else
      increment = base;

   double labelStep = increment / timescaleFactor;

   // Проверяем, достаточно ли места для текста, увеличивая шаг при необходимости
   double minTextSpacing = 50.0; // Минимальное расстояние между метками (в пикселях)
   QRectF lastLabelRect;         // Последняя нарисованная текстовая метка

   // Определяем границы рисования, используя видимую область с небольшим запасом
   double minCoord = std::max(0.0, std::floor(visibleStart / labelStep) * labelStep - labelStep);
   double maxCoord = std::ceil(visibleEnd / labelStep) * labelStep + labelStep;

   // линия с временными метками
   m_scaleScene->addLine(minCoord, -SCALE_LINE_HEIGHT / 2, maxCoord, -SCALE_LINE_HEIGHT / 2, QPen(Qt::blue, 1)); // Основная линия

   QGraphicsTextItem* label = m_scaleScene->addText("00");
   const auto minDistance = label->boundingRect().width();
   delete label;

   for (auto* it : m_lineItems)
   {
      if (it)
      {
         m_scene->removeItem(it);
      }
   }
   m_lineItems.clear();

   // Рисуем основные деления с метками только в видимой области
   for (double xCoord = minCoord; xCoord <= maxCoord; xCoord += labelStep)
   {
      QPen pen(QColor(0, 0, 255, 128), 1); // Полупрозрачный синий цвет
      pen.setCosmetic(true);

      // Добавляем метку
      uint64_t timeValue = static_cast<uint64_t>(xCoord * timescaleFactor * mulFactor);
      int adjustedUnitIndex = unitIndex; // Начинаем с текущего индекса единицы
      while (adjustedUnitIndex < units.size() - 1 && timeValue >= 1000)
      {
         timeValue /= 1000;
         ++adjustedUnitIndex; // Переходим на следующий индекс
      }
      std::string adjustedUnit = units[adjustedUnitIndex];

      QString labelText = QString::number(timeValue) + QString::fromStdString(adjustedUnit);
      if (labelText.left(1) == "0")
      {
         continue;
      }

      QGraphicsTextItem* label = m_scaleScene->addText(labelText);

      QFont font = label->font();
      font.setPointSize(8);
      label->setFont(font);
      label->setFlag(QGraphicsItem::ItemIgnoresTransformations); // Чтобы текст не масштабировался
      label->setDefaultTextColor(Qt::white);

      // Располагаем текст по центру линии
      QRectF textRect = label->boundingRect();
      double labelX = xCoord - (textRect.width() / 2) / transform().m11();

      // Проверяем, накладывается ли текущая метка на предыдущую
      if (!lastLabelRect.isEmpty() && lastLabelRect.intersects(QRectF(labelX - minDistance, -SCALE_LINE_HEIGHT - 5, textRect.width() + minDistance, textRect.height())))
      {
         // Если накладывается, пропускаем эту метку и линию
         delete label;

         auto pen = QPen(Qt::blue, 1, Qt::DashLine);
         pen.setCosmetic(true);
         m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Промежуточная линия
         continue;
      }

      label->setPos(labelX, -SCALE_LINE_HEIGHT - 5); // Центрируем текст по оси X
      lastLabelRect = QRectF(labelX, -SCALE_LINE_HEIGHT - 5, textRect.width(), textRect.height());

      // Рисуем линию только если текстовая метка была добавлена
      m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Основная линия

      std::size_t height = m_signals.size() * (WAVEFORM_HEIGHT + SPACING);
      auto* sceneLine = m_scene->addLine(xCoord, 0, xCoord, sceneRect().height(), pen); // Основная линия
      m_lineItems.push_back(sceneLine);
      sceneLine->setZValue(Layers_ScaleLine);
   }

   // Рисуем промежуточные деления только в видимой области
   double subStep = labelStep / 5.0;
   for (double xCoord = minCoord; xCoord <= maxCoord; xCoord += subStep)
   {
      if (std::fmod(xCoord, labelStep) != 0) // Пропускаем основные деления
      {
         auto pen = QPen(Qt::blue, 1, Qt::DashLine);
         pen.setCosmetic(true);
         auto line = m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Промежуточная линия
         // m_lineItems.push_back(line);
         line->setZValue(Layers_ScaleLine);
      }
   }
}

void
WaveformView::DrawDumpoffIntervals(
   QGraphicsScene* scene,
   const std::vector<std::pair<uint64_t, uint64_t>>& dumpoffIntervals)
{
   if (dumpoffIntervals.empty())
   {
      return;
   }

   for (const auto& [leftBound, rightBound] : dumpoffIntervals)
   {
      QPen pen = QPen(Qt::red, 1, Qt::SolidLine);
      QBrush brush = QBrush(QColor(255, 0, 0, 128));
      pen.setCosmetic(true);

      QRect rect(leftBound, SPACING, rightBound - leftBound, mapToScene(viewport()->rect()).boundingRect().height());
      QGraphicsRectItem* sceneRect = scene->addRect(rect, pen, brush);
      sceneRect->setZValue(2);
   }
}