#include "Include/VcdStructs.hpp"
#include "WaveItems.hpp"

#include <QDebug>
#include <iostream>

SimpleWaveItem::SimpleWaveItem(const std::shared_ptr<newVcd::Handle>& h,
                               std::shared_ptr<newVcd::SimplePinDescription> p,
                               int yOffset,
                               std::optional<std::size_t> idx,
                               QGraphicsItem* parent)
   : QGraphicsItem(parent)
   , handle(h)
   , pin(p)
   , idx(idx)
{
   setPos(0, yOffset);
   setCacheMode(DeviceCoordinateCache);

   // Предварительно строим полный путь один раз
   precalcFullPath();
}

QRectF
SimpleWaveItem::boundingRect() const
{
   return {0.0,
           0.0,
           qreal(handle->maxTs()),
           qreal(WAVEFORM_HEIGHT + SPACING)};
}

void
SimpleWaveItem::paint(QPainter* p,
                      const QStyleOptionGraphicsItem* opt,
                      QWidget*)
{
   p->setRenderHint(QPainter::Antialiasing);

   // границы по X видимой области
   const qreal x0 = std::max<qreal>(0, opt->exposedRect.left());
   const qreal x1 = std::min<qreal>(handle->maxTs(),
                                    opt->exposedRect.right());

   // вырезаем subpath между x0 и x1
   // QPainterPath visible = subPathByX(precalcedPath, x0, x1);

   QPen pen(Qt::green, 1);
   pen.setCosmetic(true);
   p->setPen(pen);
   p->drawPath(precalcedPath);

   QPen yellowPen(Qt::yellow, 1);
   yellowPen.setCosmetic(true);
   p->setPen(yellowPen);

   for (const auto& it : precalcedZPath)
   {
      p->drawPath(it);
   }

   QPen redPen = QPen(Qt::red);
   redPen.setCosmetic(true);

   QBrush darkRedBrush = QBrush(Qt::darkRed);

   p->setPen(redPen);
   p->setBrush(darkRedBrush);

   for (const auto& it : precalcedXRectangles)
   {
      p->drawRect(it);
   }
}

// строит полный путь для всей последовательности
void
SimpleWaveItem::precalcFullPath()
{
   const int yPos = SPACING;
   const int yNeg = WAVEFORM_HEIGHT;
   const int yZ = WAVEFORM_HEIGHT / 2;

   auto yFor = [&](const QString& s)
   {
      if (s == "1")
         return yPos;
      if (s == "z")
         return yZ;
      return yNeg; // "0" или "x"
   };

   std::int64_t itL = 0;
   std::int64_t itR = handle->maxTs();

   QPainterPath path;
   QPainterPath zTmpPath;
   std::vector<std::pair<std::uint64_t, std::uint64_t>> xValueRanges;
   std::uint64_t xL = 0;

   QString prev;
   // prev = pin-> ->GetPinValueAtTs(itL, pin, idx.value()); // состояние в левой границе
   prev = pin->getValueChar(itL, idx.value_or(0));

   path.moveTo(itL, yFor(prev)); // старт всегда виден
   if (yFor(prev) == yZ)
   {
      zTmpPath.moveTo(itL, yFor(prev));
   }
   uint64_t lastTs = itL;

   for (const auto& it : pin->timeline())
   {
      uint64_t ts = it.timestamp;
      QString s(pin->getValueChar(ts, idx.value_or(0)));
      // qWarning() << "[" << ts << "," << s << "]";
      if (s != prev)
      {
         const uint64_t yForPrevValue = yFor(prev);
         const uint64_t yForValue = yFor(s);

         path.lineTo(ts, yForPrevValue);
         path.lineTo(ts, yForValue);

         if (prev == "x")
         {
            xValueRanges.push_back({xL, ts});
         }
         if (s == "x")
         {
            xL = ts;
         }

         prev = s;

         if (yForPrevValue == yZ)
         {
            zTmpPath.lineTo(ts, yZ);
            precalcedZPath.push_back(zTmpPath);
            zTmpPath.clear();
         }
         if (yForValue == yZ)
         {
            zTmpPath.moveTo(ts, yForValue);
         }
      }
      else
      {
         const uint64_t yForValue = yFor(s);
         path.lineTo(ts, yForValue);

         if (yForValue == yZ)
         {
            zTmpPath.lineTo(ts, yForValue);
         }
      }
   }
   precalcedPath = std::move(path);

   for (const auto& [left, right] : xValueRanges)
   {
      QRect rect(left, yPos, right - left, WAVEFORM_HEIGHT);
      precalcedXRectangles.emplace_back(rect);
   }
}

// бинарный поиск t, чтобы path.pointAtPercent(t).x() ≈ xTarget
qreal
SimpleWaveItem::findT(const QPainterPath& path, qreal xTarget, int iters)
{
   qreal lo = 0, hi = 1;
   for (int i = 0; i < iters; ++i)
   {
      qreal mid = (lo + hi) * 0.5;
      if (path.pointAtPercent(mid).x() < xTarget)
         lo = mid;
      else
         hi = mid;
   }
   return 0.5 * (lo + hi);
}

// вырезает из path кусок между x0 и x1 (аппроксимация полилинией с sampleSteps точками)
QPainterPath
SimpleWaveItem::subPathByX(const QPainterPath& path,
                           qreal x0, qreal x1,
                           int sampleSteps)
{
   // находим параметры t0 и t1
   qreal t0 = findT(path, x0);
   qreal t1 = findT(path, x1);

   QPainterPath res;
   res.moveTo(path.pointAtPercent(t0));
   // равномерно шагаем по t
   for (int i = 1; i <= sampleSteps; ++i)
   {
      qreal t = t0 + (t1 - t0) * (qreal(i) / sampleSteps);
      res.lineTo(path.pointAtPercent(t));
   }
   return res;
}
