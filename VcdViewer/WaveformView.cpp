/************************  WaveformView.cpp  ************************/
#include "WaveformView.hpp"

#include <algorithm>
#include <cmath>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QWindow>

enum Layers
{
   Layers_ScaleLine = 1,
   Layers_Graphics,
   Layers_Cursor
};

/* === constructor ================================================= */

WaveformView::WaveformView(QWidget *parent)
    : QGraphicsView(parent)
{
   m_dpr = CalculateDevicePixelRatio();

   setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   setAlignment(Qt::AlignLeft | Qt::AlignTop);
   setMouseTracking(true);
   viewport()->setMouseTracking(true);

   createScenes();
   configureScaleView();
   setupCursor();
   initScrollSync();
   UpdateScaleViewWidth(); // стартовая ширина линейки
}

/* === scene / view bootstrap ===================================== */

void WaveformView::createScenes()
{
   m_scene = new QGraphicsScene(this);
   m_scene->setBackgroundBrush(Qt::black);
   setScene(m_scene);

   m_scaleScene = new QGraphicsScene(this);
}

void WaveformView::configureScaleView()
{
   m_scaleView = new QGraphicsView(m_scaleScene);
   m_scaleView->setFixedHeight(SCALE_LINE_HEIGHT);
   m_scaleView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   m_scaleView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   m_scaleView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
   m_scaleView->setRenderHint(QPainter::Antialiasing);
   m_scaleScene->setBackgroundBrush(Qt::black);
   m_scaleView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
}

void WaveformView::setupCursor()
{
   QPen pen(Qt::red, 1);
   pen.setCosmetic(true);
   m_cursorLine = m_scene->addLine(0, 0, 0, 0, pen);
}

void WaveformView::initScrollSync()
{
   /* синхронизация скролл-баров основного вида и линейки */
   connect(horizontalScrollBar(), &QScrollBar::valueChanged,
           m_scaleView->horizontalScrollBar(), &QScrollBar::setValue);

   connect(m_scaleView->horizontalScrollBar(), &QScrollBar::valueChanged,
           horizontalScrollBar(), &QScrollBar::setValue);

   /* обновление шкалы при скролле */
   connect(m_scaleView->horizontalScrollBar(), &QScrollBar::valueChanged,
           this, &WaveformView::DrawScaleLine);
   connect(m_scaleView->verticalScrollBar(), &QScrollBar::valueChanged,
           this, &WaveformView::DrawScaleLine);

   /* изменение ширины линейки при вертикальном скролле */
   connect(verticalScrollBar(), &QScrollBar::valueChanged,
           this, &WaveformView::UpdateScaleViewWidth);

   /* снап-скролл для вертикали */
   auto *sb = new SnapScrollBar(Qt::Vertical, this);
   sb->setSnapStep(WAVEFORM_HEIGHT + SPACING);
   setVerticalScrollBar(sb);
}

/* === events ====================================================== */

void WaveformView::mousePressEvent(QMouseEvent *e)
{
   if (e->button() == Qt::LeftButton && m_cursorLine)
   {
      const QPointF scenePos = mapToScene(e->pos());
      m_cursorPos = scenePos.x();
      updateCursorGeometry();
      emit SelectedTimestampChange(static_cast<uint64_t>(scenePos.x()));
   }
   QGraphicsView::mousePressEvent(e);
}

void WaveformView::mouseMoveEvent(QMouseEvent *e)
{
   emit PointerPositionChanged(static_cast<uint64_t>(mapToScene(e->pos()).x()));
   QGraphicsView::mouseMoveEvent(e);
}

void WaveformView::wheelEvent(QWheelEvent *e)
{
   if (horizontalScrollBar()->isVisible())
   {
      constexpr int step = 15; // px per detent
      auto *sb = horizontalScrollBar();
      int newVal = sb->value() - e->angleDelta().y() / 120 * step;
      sb->setValue(std::clamp(newVal, sb->minimum(), sb->maximum()));
   }
   e->accept();
}

void WaveformView::resizeEvent(QResizeEvent *e)
{
   if (m_dumpoffItem)
      m_dumpoffItem->setHeight(height());

   QGraphicsView::resizeEvent(e);
   UpdateScaleViewWidth();
   UpdateSignals(m_signals);
}

/* === public API ================================================== */

void WaveformView::ZoomIn() { ZoomX(m_currentZoomLevel + 1); }
bool WaveformView::ZoomOut()
{
   if (!HasScrollBar())
      return false;
   ZoomX(m_currentZoomLevel - 1);
   DrawScaleLine();
   return true;
}

void WaveformView::SetInitialScale()
{
   zoomToRange(0, m_handle->GetMaxTs());
}

void WaveformView::SetHandle(std::shared_ptr<vcd::Handle> newHandle)
{
   m_handle = std::move(newHandle);

   /* очистка старого состояния */
   if (m_dumpoffItem)
   {
      delete m_dumpoffItem;
      m_dumpoffItem = nullptr;
   }
   m_scene->clear();
   m_scaleScene->clear();
   m_signals.clear();
   m_aliasItemMap.clear();
   m_lineItems.clear();
   m_aliasOrder.clear();
   m_expandedMap.clear();
   m_currentZoomLevel = 0;
   m_minZoomLevel = 0;
   m_cursorPos = 0;
   m_currentScaleValue = 0;

   m_scaleView->resetTransform();
   resetTransform();
   setupCursor();

   if (!m_handle)
   {
      m_scene->setSceneRect(0, 0, 0, height());
      return;
   }

   DrawScaleLine(true);
   m_scene->setSceneRect(0, 0, m_handle->GetMaxTs(), height());

   if (!m_handle->GetDumpoffIntervals().empty())
   {
      m_dumpoffItem = new DumpoffItem(m_handle->GetDumpoffIntervals(), m_handle);
      m_dumpoffItem->setHeight(height());
      m_scene->addItem(m_dumpoffItem);
   }
}

void WaveformView::OnItemExpandedOrCollapsed(vcd::PinDescriptionPtr pin, bool isExpanded)
{
   m_expandedMap[pin] = isExpanded;
   auto *multiItem = static_cast<MultipleWaveItem *>(m_aliasItemMap.at(std::string(pin->GetAlias())));
   multiItem->SetExpanded(isExpanded);

   int yOff = 0;
   for (const auto &alias : m_aliasOrder)
   {
      auto *item = m_aliasItemMap.at(alias);
      item->setPos(0, yOff);
      yOff += item->boundingRect().height();
   }
}

void WaveformView::UpdateSignals(std::vector<vcd::PinDescriptionPtr> newSignals)
{
   if (!m_handle)
      return;

   /* удалить элементы, которых больше нет */
   for (auto it = m_aliasItemMap.begin(); it != m_aliasItemMap.end();)
   {
      bool stillPresent = std::any_of(newSignals.begin(), newSignals.end(),
                                      [&](auto &p)
                                      { return p->GetAlias() == it->first; });
      if (!stillPresent)
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
   int yOff = 0;

   /* добавить новые / пере-позиционировать старые */
   for (auto &pin : newSignals)
   {
      const std::string alias(pin->GetAlias());

      if (!m_aliasItemMap.count(alias))
      {
         QGraphicsItem *item = nullptr;

         if (pin->GetPinType() == vcd::PinType::parameter)
            item = new ParamWaveItem(m_handle, std::static_pointer_cast<vcd::ParamPinDescription>(pin), yOff);
         else if (pin->GetSignalType() == vcd::SignalType::simple)
            item = new SimpleWaveItem(m_handle, std::static_pointer_cast<vcd::SimplePinDescription>(pin), yOff);
         else
         {
            item = new MultipleWaveItem(m_handle, std::static_pointer_cast<vcd::BusPinDescription>(pin), yOff, CalcScaleCoeff());
            connect(this, &WaveformView::ScaleCoeffChanged,
                    static_cast<MultipleWaveItem *>(item), &MultipleWaveItem::SetScaleCoeff);
         }
         m_scene->addItem(item);
         m_aliasItemMap[alias] = item;
      }

      m_aliasItemMap[alias]->setPos(0, yOff);
      yOff += m_aliasItemMap[alias]->boundingRect().height();
      m_aliasOrder.push_back(alias);
   }
   m_signals = std::move(newSignals);

   /* обновить границы сцены и курсор */
   m_scene->setSceneRect(0, 0, m_handle->GetMaxTs(), std::max(yOff, height()));
   DrawScaleLine();
   updateCursorGeometry();
}

void WaveformView::UpdateScaleViewWidth()
{
   const int vBarW = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
   Q_UNUSED(vBarW); // пока не используем, но держим на будущее
   m_scaleView->setFixedWidth(viewport()->width());
}

QGraphicsView *WaveformView::GetScaleView() { return m_scaleView; }

/* === zoom helpers =============================================== */

void WaveformView::ZoomX(int level)
{
   const qreal factor = 1.0 + (level - m_currentZoomLevel) * 0.1;
   scale(factor, 1.0);
   m_scaleView->scale(factor, 1.0);
   m_currentZoomLevel = level;
   emit ScaleCoeffChanged(CalcScaleCoeff());
   DrawScaleLine();
}

double WaveformView::CalcScaleCoeff() const
{
   const QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();
   return visibleRect.width() / width();
}

qreal WaveformView::CalculateDevicePixelRatio() const
{
   QScreen *s = windowHandle() ? windowHandle()->screen()
                               : QGuiApplication::primaryScreen();
   return s ? s->devicePixelRatio() : 1.0;
}

void WaveformView::zoomToRange(uint64_t x0, uint64_t x1)
{
   if (!scene() || x1 <= x0)
      return;

   const qreal viewW = viewport()->width();
   const qreal newSx = viewW / qreal(x1 - x0);
   const qreal curSx = transform().m11();
   const qreal factor = newSx / curSx;

   scale(factor, 1.0);
   m_scaleView->scale(factor, 1.0);

   const qreal center = (x0 + x1) * 0.5;
   centerOn(center, 0);
   m_scaleView->centerOn(center, 0);

   m_currentZoomLevel = 0;
   emit ScaleCoeffChanged(CalcScaleCoeff());
   DrawScaleLine();
}

/* === helpers ===================================================== */

void WaveformView::updateCursorGeometry()
{
   if (!m_cursorLine)
      return;

   const std::size_t h = std::max<std::size_t>(height(),
                                               m_signals.size() * (WAVEFORM_HEIGHT + SPACING));
   m_cursorLine->setLine(m_cursorPos, 0, m_cursorPos, h);
   m_cursorLine->setZValue(Layers_Cursor);
}

void WaveformView::clearScaleLines()
{
   for (auto *l : m_lineItems)
      if (l)
         m_scene->removeItem(l);
   m_lineItems.clear();
}

void WaveformView::DrawScaleLine(bool reset /* = false */)
{
   if (!m_handle)
      return;

   /* ---------- 1. Видимые границы окна -------------------------- */
   double vStart, vEnd, vWidth;
   if (reset)
   {
      vStart = 0.0;
      vEnd = m_handle->GetMaxTs();
      if (vEnd == 0.0)
         return;
      vWidth = vEnd;
   }
   else
   {
      const QRectF vr = mapToScene(viewport()->rect()).boundingRect();
      vStart = vr.left();
      vEnd = std::min<double>(vr.right(), m_handle->GetMaxTs());
      vWidth = vEnd - vStart;
   }

   /* ---------- 2. Подготовка сцен ------------------------------- */
   m_scaleScene->clear();
   m_scaleScene->setSceneRect(0, -SCALE_LINE_HEIGHT,
                              m_handle->GetMaxTs(), SCALE_LINE_HEIGHT);

   static const std::unordered_map<std::string, double> factor{
       {"ps", 1.0}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}};
   static const std::vector<std::string> units{"ps", "ns", "us", "ms", "s"};

   /* base #timescale VCD (напр. "10ns") */
   const std::string ts = std::string(m_handle->GetTimeScale());
   auto itNonDigit = std::find_if(ts.begin(), ts.end(),
                                  [](char c)
                                  { return !std::isdigit(static_cast<unsigned char>(c)); });

   const std::size_t mul = std::stoul(std::string{ts.substr(0, size_t(itNonDigit - ts.begin()))});
   const std::string baseUnit(itNonDigit, ts.end());

   const double sceneUnitPs = mul * factor.at(baseUnit); // 1 unit сцены = ? пс

   /* ---------- 3. «Красивый» шаг деления ------------------------ */
   const double roughStepPs = (vWidth * sceneUnitPs) / 10.0; // ≈10 делений
   double decade = std::pow(10.0, std::floor(std::log10(roughStepPs)));
   double nicePs = decade;
   if (roughStepPs / decade > 5)
      nicePs = 10 * decade;
   else if (roughStepPs / decade > 2)
      nicePs = 5 * decade;
   else if (roughStepPs / decade > 1)
      nicePs = 2 * decade;

   const double stepScene = nicePs / sceneUnitPs;

   /* ---------- 4. Диапазон рисования ---------------------------- */
   double minX = std::floor(vStart / stepScene) * stepScene - stepScene;
   double maxX = std::ceil(vEnd / stepScene) * stepScene + stepScene;
   maxX = std::min<double>(maxX, m_handle->GetMaxTs());

   m_scaleScene->addLine(minX, -SCALE_LINE_HEIGHT / 2,
                         maxX, -SCALE_LINE_HEIGHT / 2,
                         QPen(Qt::blue, 1));

   /* ---------- 5. Подготовка коллизий текста -------------------- */
   auto *probe = m_scaleScene->addText("00");
   const double minDX = probe->boundingRect().width();
   delete probe;

   for (auto *l : m_lineItems)
      if (l)
         m_scene->removeItem(l);
   m_lineItems.clear();
   QRectF lastLbl;

   /* ---------- 6. Основные деления ------------------------------ */
   for (double x = minX; x <= maxX + 1e-6; x += stepScene)
   {
      const double timePs = x * sceneUnitPs;

      /* --- выбираем единицу так, чтобы в подписи ≤ 1 знака после запятой --- */
      int uIdx = int(units.size()) - 1; // начинаем с крупнейшей (s)
      int prec = 0;
      while (uIdx > 0)
      {
         double val = timePs / factor.at(units[uIdx]);
         double val10 = val * 10.0;

         bool oneDecimal = std::fabs(val10 - std::round(val10)) < 1e-6;
         bool zeroDecimal = std::fabs(val - std::round(val)) < 1e-6;

         if (val >= 1.0 && (zeroDecimal || oneDecimal))
         {
            prec = zeroDecimal ? 0 : 1;
            break; // нашли подходящий юнит
         }
         --uIdx; // пробуем более мелкий
      }
      /* если дошли до ps */
      double value = timePs / factor.at(units[uIdx]);
      if (prec == 0 && std::fabs(value - std::round(value)) > 1e-6)
         prec = 2; // в ps/нс могут остаться «.25»

      const QString txt = QString::number(value, 'f', prec) +
                          QString::fromStdString(units[uIdx]);

      /* --- текст + вертикальная линия --------------------------- */
      auto *lbl = m_scaleScene->addText(txt);
      auto f = lbl->font();
      f.setPointSize(8);
      lbl->setFont(f);
      lbl->setDefaultTextColor(Qt::white);
      lbl->setFlag(QGraphicsItem::ItemIgnoresTransformations);

      const QRectF br = lbl->boundingRect();
      const double lx = x - br.width() / 2.0 / transform().m11();

      if (!lastLbl.isEmpty() &&
          lastLbl.intersects(QRectF(lx - minDX,
                                    -SCALE_LINE_HEIGHT - 5,
                                    br.width() + minDX, br.height())))
      {
         delete lbl; // тесновато: подпись опускаем
         QPen dash(Qt::blue, 1, Qt::DashLine);
         dash.setCosmetic(true);
         m_scaleScene->addLine(x, -SCALE_LINE_HEIGHT / 2, x, SCALE_LINE_HEIGHT, dash);
      }
      else
      {
         lbl->setPos(lx, -SCALE_LINE_HEIGHT - 5);
         lastLbl = {lx, -SCALE_LINE_HEIGHT - 5, br.width(), br.height()};

         QPen pen(QColor(0, 0, 255, 128), 1);
         pen.setCosmetic(true);
         m_scaleScene->addLine(x, -SCALE_LINE_HEIGHT / 2, x, SCALE_LINE_HEIGHT, pen);

         auto *l = m_scene->addLine(x, 0, x, sceneRect().height(), pen);
         l->setZValue(Layers_ScaleLine);
         m_lineItems.push_back(l);
      }
   }

   /* ---------- 7. Прерывистые промежуточные деления ------------- */
   const double sub = stepScene / 5.0;
   for (double x = minX; x <= maxX + 1e-6; x += sub)
   {
      if (std::fmod(x - minX, stepScene) < 1e-6)
         continue; // это уже основная риска
      QPen p(Qt::blue, 1, Qt::DashLine);
      p.setCosmetic(true);
      auto *l = m_scaleScene->addLine(x, -SCALE_LINE_HEIGHT / 2,
                                      x, SCALE_LINE_HEIGHT, p);
      l->setZValue(Layers_ScaleLine);
   }
}
