#include "WaveItems.hpp"

QRectF DumpoffItem::boundingRect() const
{
   return {0.0,
           0.0,
           qreal(m_handle->GetMaxTs()),
           m_height};
}

void DumpoffItem::paint(QPainter *p,
                        const QStyleOptionGraphicsItem *opt,
                        QWidget *)
{
   if (m_ranges.empty())
   {
      return;
   }

   QPen pen = QPen(Qt::red, 1, Qt::SolidLine);
   QBrush brush = QBrush(QColor(255, 0, 0, 128));
   pen.setCosmetic(true);
   p->setPen(pen);
   p->setBrush(brush);
   for (const auto &[leftBound, rightBound] : m_ranges)
   {
      QRect rect(leftBound, SPACING, rightBound - leftBound, opt->exposedRect.height());
      p->drawRect(rect);
   }
}
