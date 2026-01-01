#include "Include/VcdStructs.hpp"
#include "WaveItems.hpp"

#include <QDebug>
#include <iostream>

SimpleWaveItem::SimpleWaveItem(const std::shared_ptr<vcd::Handle> &h,
                               std::shared_ptr<vcd::SimplePinDescription> p,
                               int yOffset,
                               std::optional<std::size_t> idx,
                               QGraphicsItem *parent)
    : QGraphicsItem(parent), m_handle(h), m_pin(p), m_idx(idx)
{
   setPos(0, yOffset);
   // setCacheMode(DeviceCoordinateCache);

   // Предвар ительно строим полный путь один раз
   PrecalcFullPath();
}

QRectF
SimpleWaveItem::boundingRect() const
{
   return {0.0,
           0.0,
           qreal(m_handle->GetMaxTs()),
           qreal(WAVEFORM_HEIGHT + SPACING)};
}

void SimpleWaveItem::paint(QPainter *p,
                           const QStyleOptionGraphicsItem *opt,
                           QWidget *)
{
   p->setRenderHint(QPainter::Antialiasing);
   p->setClipRect(opt->exposedRect);
   // границы по X видимой области
   const qreal x0 = std::max<qreal>(0, opt->exposedRect.left());
   const qreal x1 = std::min<qreal>(m_handle->GetMaxTs(),
                                    opt->exposedRect.right());

   QPen pen(Qt::green, 1);
   pen.setCosmetic(true);
   p->setPen(pen);
   p->drawPath(m_precalcedPath);

   QPen yellowPen(Qt::yellow, 1);
   yellowPen.setCosmetic(true);
   p->setPen(yellowPen);

   for (const auto &it : m_precalcedZPath)
   {
      p->drawPath(it);
   }

   QPen redPen = QPen(Qt::red);
   redPen.setCosmetic(true);

   QBrush darkRedBrush = QBrush(Qt::darkRed);

   p->setPen(redPen);
   p->setBrush(darkRedBrush);

   for (const auto &it : m_precalcedXRectangles)
   {
      p->drawRect(it);
   }
}

// строит полный путь для всей последовательности
void SimpleWaveItem::PrecalcFullPath()
{
   const int yPos = SPACING;
   const int yNeg = WAVEFORM_HEIGHT;
   const int yZ = WAVEFORM_HEIGHT / 2;

   auto yFor = [&](const QString &s)
   {
      if (s == "1")
         return yPos;
      if (s == "z")
         return yZ;
      return yNeg; // "0" или "x"
   };

   std::int64_t itL = 0;
   std::int64_t itR = m_handle->GetMaxTs();

   QPainterPath path;
   QPainterPath zTmpPath;
   std::vector<std::pair<std::uint64_t, std::uint64_t>> xValueRanges;
   std::uint64_t xL = 0;

   QString prev;
   prev = m_pin->GetValueChar(itL, m_idx.value_or(0));

   path.moveTo(itL, yFor(prev)); // старт всегда виден
   if (yFor(prev) == yZ)
   {
      zTmpPath.moveTo(itL, yFor(prev));
   }
   uint64_t lastTs = itL;

   QString s;
   if (m_pin->GetTimeline().empty())
   {
      const char initState = m_pin->GetInitState()[0];
      if (initState == '0')
      {
         path.moveTo(0, yNeg);
         path.lineTo(m_handle->GetMaxTs(), yNeg);
      }
      else if (initState == '1')
      {
         path.moveTo(0, yPos);
         path.lineTo(m_handle->GetMaxTs(), yPos);
      }
      else if (initState == 'z')
      {
         zTmpPath.moveTo(0, yZ);
         zTmpPath.lineTo(m_handle->GetMaxTs(), yZ);
      }
      else if (initState == 'x')
      {
         xValueRanges.push_back({0, m_handle->GetMaxTs()});
      }
   }

   for (const auto &it : m_pin->GetTimeline())
   {
      uint64_t ts = it.timestamp;
      s = (m_pin->GetValueChar(ts, m_idx.value_or(0)));
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
         m_precalcedZPath.push_back(zTmpPath);
         zTmpPath.clear();
      }
      if (yForValue == yZ)
      {
         zTmpPath.moveTo(ts, yForValue);
      }
   }

   if (s == "0")
   {
      path.lineTo(m_handle->GetMaxTs(), yNeg);
   }
   else if (s == "1")
   {
      path.lineTo(m_handle->GetMaxTs(), yPos);
   }
   else if (s == "z")
   {
      zTmpPath.lineTo(m_handle->GetMaxTs(), yZ);
   }
   else if (s == "x")
   {
      xValueRanges.push_back({m_pin->GetTimeline().rbegin()->timestamp, m_handle->GetMaxTs()});
   }

   m_precalcedPath = std::move(path);
   qWarning() << m_precalcedPath;

   for (const auto &[left, right] : xValueRanges)
   {
      QRect rect(left, yPos, right - left, WAVEFORM_HEIGHT);
      m_precalcedXRectangles.emplace_back(rect);
   }
}
