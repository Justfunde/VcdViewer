#include "Include/VcdStructs.hpp"
#include "WaveItems.hpp"

#include <QPainter>
#include <map>
#include <cmath>
#include <algorithm>

/*───────────────────────── helpers ──────────────────────────*/
namespace
{
   inline int yFor(char v) noexcept
   {
      return v == '1'   ? SPACING
             : v == 'z' ? WAVEFORM_HEIGHT / 2
                        : WAVEFORM_HEIGHT; // '0' или 'x'
   }
} // namespace

/*──────────────────────── ctor ──────────────────────────────*/
SimpleWaveItem::SimpleWaveItem(const std::shared_ptr<newVcd::Handle> &h,
                               std::shared_ptr<newVcd::SimplePinDescription> p,
                               int yOffset,
                               std::optional<std::size_t> bit,
                               QGraphicsItem *parent)
    : QGraphicsItem(parent), handle(h), pin(std::move(p)), idx(bit)
{
   setPos(0, yOffset);
   // setCacheMode(ItemCoordinateCache); // самый дешёвый кэш

   buildSegments();
}

/*──────────────────────── boundingRect ─────────────────────*/
QRectF SimpleWaveItem::boundingRect() const
{
   return {0, 0,
           qreal(handle->maxTs()),
           qreal(WAVEFORM_HEIGHT + SPACING)};
}

/*────────────────────── buildSegments() ────────────────────*/
void SimpleWaveItem::buildSegments()
{
   const auto &tl = pin->timeline();
   const auto endT = handle->maxTs();
   const size_t b = idx.value_or(0);

   char prevVal = pin->initState().front();
   quint64 prevTs = 0;
   quint64 xLeft = 0;

   auto push = [&](quint64 l, quint64 r, char v)
   {
      if (l < r)
         segs_.push_back({l, r, v});
   };

   if (tl.empty()) /*–– без переходов –*/
   {
      if (prevVal == 'x')
         xRects_.emplace_back(QRect(0, SPACING, endT, WAVEFORM_HEIGHT));
      else
         push(0, endT, prevVal);
      return;
   }

   for (const auto &ev : tl)
   {
      const quint64 t = ev.timestamp;
      const char val = pin->getValueChar(t, b);

      push(prevTs, t, prevVal);

      if (prevVal == 'x' && val != 'x')
         xRects_.emplace_back(QRect(xLeft, SPACING, t - xLeft, WAVEFORM_HEIGHT));
      if (prevVal != 'x' && val == 'x')
         xLeft = t;

      prevVal = val;
      prevTs = t;
   }

   /* хвост */
   if (prevVal == 'x')
      xRects_.emplace_back(QRect(prevTs, SPACING, endT - prevTs, WAVEFORM_HEIGHT));
   else
      push(prevTs, endT, prevVal);
}

/*──────────────────────── paint() ──────────────────────────*/
void SimpleWaveItem::paint(QPainter *p,
                           const QStyleOptionGraphicsItem *opt,
                           QWidget *)
{
   p->setRenderHint(QPainter::Antialiasing, false);
   renderVisible(p, opt);
}

/*───────────────────── renderVisible() ─────────────────────*/
#include <QElapsedTimer>
#include <QDebug>

/*───────────────────── renderVisible() ─────────────────────*/
#include <QElapsedTimer>
#include <QDebug>

void SimpleWaveItem::renderVisible(QPainter *p,
                                   const QStyleOptionGraphicsItem *opt)
{
   QElapsedTimer tAll;
   tAll.start();

   /* 1 ─ видимые границы -------------------------------------------------- */
   QElapsedTimer t;
   t.start();
   const quint64 visL = quint64(std::max<qreal>(0.0, opt->exposedRect.left()));
   const quint64 visR = quint64(std::min<qreal>(qreal(handle->maxTs()),
                                                opt->exposedRect.right()));
   if (visL >= visR)
      return;
   qWarning() << "SWI-render: step1-bounds —" << t.elapsed() << "ms";

   /* 2 ─ масштаб ---------------------------------------------------------- */
   t.restart();
   /* 2. масштаб X -------------------------------------------------------- */
   const qreal pxPerTick = p->worldTransform().m11(); // px / tick
   const qreal worldStep = 1.0 / pxPerTick;           // tick-ов в одном px

   /* ширина видимой области в ЭКРАННЫХ пикселях  */
   const int pixelW =
       std::max<int>(1, int(std::ceil(opt->exposedRect.width() * pxPerTick)));
   qWarning() << "SWI-render: step2-scale  —" << t.elapsed() << "ms";

   /* 3 ─ buckets: плоские массивы ---------------------------------------- */
   t.restart();
   struct Bucket
   {
      bool z = false, v0 = false, v1 = false;
   };

   std::vector<Bucket> buckets; // переиспользуем память
   buckets.assign(pixelW, {});  // обнулить O(width)

   std::vector<int> cols;    // индексы изменённых колонок
   cols.reserve(pixelW / 4); // грубо 25 % окна
   qWarning() << "SWI-render: step3-bucket-reset —" << t.elapsed() << "ms";

   /* helper ─ вывод содержимого buckets ---------------------------------- */
   auto flushBuckets = [&]()
   {
      static QPen thinPen(Qt::green, 1);
      thinPen.setCosmetic(true);
      p->setPen(thinPen);
      p->setBrush(Qt::NoBrush);

      if (cols.empty())
         return;

      std::sort(cols.begin(), cols.end());
      cols.erase(std::unique(cols.begin(), cols.end()), cols.end());

      int runStart = -1, prevCol = -2;
      auto flushRun = [&](int colAfter)
      {
         if (runStart < 0)
            return;
         const qreal x0 = opt->exposedRect.left() + runStart * worldStep;
         const qreal w = (colAfter - runStart) * worldStep;
         p->fillRect(QRectF(x0, yFor('1') - 0.5, w, 1.0), Qt::green);
         runStart = -1;
      };

      for (int col : cols)
      {
         const Bucket &b = buckets[col];
         const bool pure1 = b.v1 && !b.v0 && !b.z;

         if (pure1) /* накапливаем полосу «1» */
         {
            if (runStart < 0)
               runStart = col;
            else if (col != prevCol + 1)
            {
               flushRun(col);
               runStart = col;
            }
            prevCol = col;
         }
         else /* выводим одиночную колонку */
         {
            flushRun(col);
            const qreal x0 = opt->exposedRect.left() + col * worldStep;
            const qreal x1 = x0 + worldStep;

            if (b.v0)
               p->drawLine(QLineF(x0, yFor('0'), x1, yFor('0')));
            if (b.z)
               p->drawLine(QLineF(x0, yFor('z'), x1, yFor('z')));
            if (b.v0 && b.v1)
               p->drawLine(QLineF(x0, yFor('1'), x1, yFor('1')));
         }
      }
      flushRun(cols.back() + 1);
      cols.clear();
   };

   /* 4 ─ обход segs_ ------------------------------------------------------ */
   t.restart();
   auto first = std::lower_bound(
       segs_.begin(), segs_.end(), visL,
       [](const LineSeg &s, quint64 ts)
       { return s.rightTs < ts; });

   static QPen horzPen(Qt::green, 1);
   horzPen.setCosmetic(true);
   p->setPen(horzPen);

   char prevVal = 0;
   bool firstSeg = true;

   for (auto it = first; it != segs_.end() && it->leftTs < visR; ++it)
   {
      quint64 l = std::max<quint64>(it->leftTs, visL);
      quint64 r = std::min<quint64>(it->rightTs, visR);
      if (l >= r)
         continue;

      if (!firstSeg && it->value != prevVal)
      { /* вертикальный переход */
         flushBuckets();
         p->drawLine(QLineF(l, yFor(prevVal), l, yFor(it->value)));
      }
      firstSeg = false;
      prevVal = it->value;

      const int colL = int((l - visL) * pxPerTick);
      const int colR = int((r - visL) * pxPerTick);

      if (colL == colR) /* узкий сегмент → bucket */
      {
         Bucket &b = buckets[colL];
         if (it->value == '0')
            b.v0 = true;
         else if (it->value == '1')
            b.v1 = true;
         else if (it->value == 'z')
            b.z = true;
         cols.push_back(colL);
      }
      else /* широкий сегмент → сразу */
      {
         flushBuckets();
         p->drawLine(QLineF(l, yFor(it->value), r, yFor(it->value)));
      }
   }
   qWarning() << "SWI-render: step4-segments —" << t.elapsed() << "ms";

   flushBuckets(); /* окончательный сброс */

   /* 5 ─ X-участки -------------------------------------------------------- */
   t.restart();
   static QPen xPen(Qt::red, 1);
   xPen.setCosmetic(true);
   static QBrush xBrush(Qt::darkRed);
   p->setPen(xPen);
   p->setBrush(xBrush);

   for (const auto &rect : xRects_)
      if (rect.right() >= visL && rect.left() <= visR)
         p->drawRect(rect);
   qWarning() << "SWI-render: step5-drawX —" << t.elapsed() << "ms";

   /* итог ----------------------------------------------------------------- */
   qWarning() << "SWI-render: TOTAL —" << tAll.elapsed() << "ms";
}

/*─────────────────── заглушки findT / subPathByX ──────────────────*/
qreal SimpleWaveItem::findT(const QPainterPath &, qreal, int) { return 0; }
QPainterPath SimpleWaveItem::subPathByX(const QPainterPath &, qreal, qreal, int)
{
   return {};
}