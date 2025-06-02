#include "Include/VcdStructs.hpp"
#include "WaveItems.hpp"

#include <QPainter>
#include <algorithm>
#include <vector>

//------------------------------------------------------------------------------------------------------
// Вспомогательные утилиты
//------------------------------------------------------------------------------------------------------
namespace
{
   /// Преобразование логического состояния в координату по Y.
   int yFor(const QString &v) noexcept
   {
      return v == "1"   ? SPACING             // high
             : v == "z" ? WAVEFORM_HEIGHT / 2 // high-Z
                        : WAVEFORM_HEIGHT;    // low или unknown
   }

   /// Сброс временного фрагмента «Z»-сигнала в результирующий контейнер.
   inline void flushZ(QPainterPath &fragment,
                      std::vector<QPainterPath> &dst) // NOLINT(modernize-pass-by-value)
   {
      if (!fragment.isEmpty())
      {
         dst.emplace_back(std::move(fragment));
         fragment = {}; // очищаем
      }
   }
} // namespace

//------------------------------------------------------------------------------------------------------
// Конструктор и базовое API Qt
//------------------------------------------------------------------------------------------------------
SimpleWaveItem::SimpleWaveItem(const std::shared_ptr<newVcd::Handle> &h,
                               std::shared_ptr<newVcd::SimplePinDescription> p,
                               int yOffset,
                               std::optional<std::size_t> idx_,
                               QGraphicsItem *parent)
    : QGraphicsItem(parent),
      handle(h),
      pin(std::move(p)),
      idx(idx_)
{
   setPos(0, yOffset);
   setCacheMode(DeviceCoordinateCache);

   precalcFullPath();
}

QRectF SimpleWaveItem::boundingRect() const
{
   return {0.0,
           0.0,
           qreal(handle->maxTs()),
           qreal(WAVEFORM_HEIGHT + SPACING)};
}

//------------------------------------------------------------------------------------------------------
// Отрисовка
//------------------------------------------------------------------------------------------------------
void SimpleWaveItem::paint(QPainter *p,
                           const QStyleOptionGraphicsItem *,
                           QWidget *)
{
   p->setRenderHint(QPainter::Antialiasing);

   // основная линия «0/1»
   QPen wavePen(Qt::green, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
   wavePen.setCosmetic(true);
   p->setPen(wavePen);
   p->drawPath(precalcedPath);

   // «Z»-участки
   QPen zPen(Qt::yellow, 1);
   zPen.setCosmetic(true);
   p->setPen(zPen);
   for (const auto &fragment : precalcedZPath)
      p->drawPath(fragment);

   // «X»-прямоугольники
   QPen xPen(Qt::red);
   xPen.setCosmetic(true);
   QBrush xBrush(Qt::darkRed);

   p->setPen(xPen);
   p->setBrush(xBrush);
   for (const auto &rect : precalcedXRectangles)
      p->drawRect(rect);
}

//------------------------------------------------------------------------------------------------------
// Предварительная генерация геометрии
//------------------------------------------------------------------------------------------------------
void SimpleWaveItem::precalcFullPath()
{
   const auto &timeline = pin->timeline();
   const auto tsRight = handle->maxTs();
   const size_t bitIdx = idx.value_or(0);

   // 1) Особый случай — переходов нет.
   if (timeline.empty())
   {
      const char init = pin->initState().front();
      if (init == 'x')
      {
         precalcedXRectangles.emplace_back(QRect(0, SPACING, tsRight, WAVEFORM_HEIGHT));
         return;
      }
      if (init == 'z')
      {
         QPainterPath zPath;
         zPath.moveTo(0, yFor("z"));
         zPath.lineTo(tsRight, yFor("z"));
         precalcedZPath.emplace_back(std::move(zPath));
         return;
      }

      QPainterPath path;
      path.moveTo(0, yFor(QString(init)));
      path.lineTo(tsRight, yFor(QString(init)));
      precalcedPath = std::move(path);
      return;
   }

   // 2) Обычный случай — есть переходы.
   QPainterPath mainPath;
   QPainterPath zFragment;
   std::vector<QRect> xRects;

   QString prev = QString::fromStdString(pin->initState());
   uint64_t xLeft = 0;

   mainPath.moveTo(0, yFor(prev));
   if (prev == "z")
      zFragment.moveTo(0, yFor(prev));
   else if (prev == "x")
      xLeft = 0;

   for (const auto &e : timeline)
   {
      const uint64_t ts = e.timestamp;
      const QString val(pin->getValueChar(ts, bitIdx));

      // горизонталь до ts
      mainPath.lineTo(ts, yFor(prev));
      // вертикаль на новый уровень
      mainPath.lineTo(ts, yFor(val));

      // «X»
      if (prev == "x" && val != "x")
         xRects.emplace_back(QRect(xLeft, SPACING, ts - xLeft, WAVEFORM_HEIGHT));
      if (prev != "x" && val == "x")
         xLeft = ts;

      // «Z»
      if (prev == "z" && val != "z")
      {
         zFragment.lineTo(ts, yFor(prev));
         flushZ(zFragment, precalcedZPath);
      }
      if (prev != "z" && val == "z")
         zFragment.moveTo(ts, yFor(val));

      prev = val;
   }

   // хвост последнего состояния
   if (prev == "0" || prev == "1")
      mainPath.lineTo(tsRight, yFor(prev));
   else if (prev == "z")
   {
      zFragment.lineTo(tsRight, yFor(prev));
      flushZ(zFragment, precalcedZPath);
   }
   else if (prev == "x")
      xRects.emplace_back(QRect(timeline.back().timestamp,
                                SPACING,
                                tsRight - timeline.back().timestamp,
                                WAVEFORM_HEIGHT));

   precalcedPath = std::move(mainPath);
   precalcedXRectangles = std::move(xRects);
}

//------------------------------------------------------------------------------------------------------
// Вспомогательные функции (без изменений, кроме чистого кода)
//------------------------------------------------------------------------------------------------------
qreal SimpleWaveItem::findT(const QPainterPath &path, qreal xTarget, int iters)
{
   qreal lo = 0, hi = 1;
   for (int i = 0; i < iters; ++i)
   {
      const qreal mid = 0.5 * (lo + hi);
      (path.pointAtPercent(mid).x() < xTarget) ? lo = mid : hi = mid;
   }
   return 0.5 * (lo + hi);
}

QPainterPath SimpleWaveItem::subPathByX(const QPainterPath &path,
                                        qreal x0,
                                        qreal x1,
                                        int samples)
{
   const auto t0 = findT(path, x0);
   const auto t1 = findT(path, x1);

   QPainterPath out;
   out.moveTo(path.pointAtPercent(t0));
   for (int i = 1; i <= samples; ++i)
   {
      const qreal t = t0 + (t1 - t0) * (qreal(i) / samples);
      out.lineTo(path.pointAtPercent(t));
   }
   return out;
}
