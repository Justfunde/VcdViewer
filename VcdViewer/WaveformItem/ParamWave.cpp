
#include "WaveItems.hpp"

ParamWaveItem::ParamWaveItem(const std::shared_ptr<vcd::Handle> &h,
                             std::shared_ptr<vcd::ParamPinDescription> p,
                             int yOffset,
                             QGraphicsItem *parent)
    : QGraphicsItem(parent), m_handle(h), m_pin(p)
{
   setPos(0, yOffset);
   // setCacheMode(DeviceCoordinateCache);
   m_label = new QGraphicsSimpleTextItem(this); // дочерний объект
   m_label->setFlag(QGraphicsItem::ItemIgnoresTransformations);
   m_label->setBrush(Qt::white);
   QFont f("Monospace");
   f.setPixelSize(15);
   m_label->setFont(f);
}

QRectF
ParamWaveItem::boundingRect() const
{
   return {0.0,
           0.0,
           qreal(m_handle->GetMaxTs()),
           qreal(WAVEFORM_HEIGHT)};
}

void ParamWaveItem::paint(QPainter *p,
                          const QStyleOptionGraphicsItem *opt,
                          QWidget *)
{
   /* ---------- обычная отрисовка волны ------------- */
   const qreal x0 = qMax<qreal>(0, opt->exposedRect.left());
   const qreal x1 = qMin<qreal>(m_handle->GetMaxTs(),
                                opt->exposedRect.right());

   const int yPos = SPACING;
   const int yNeg = WAVEFORM_HEIGHT;

   QPen lvl(Qt::green, 1);
   lvl.setCosmetic(true);
   p->setPen(lvl);
   p->drawLine(x0, yPos, x1, yPos);
   p->drawLine(x0, yNeg, x1, yNeg);

   /* ---------- текстовая подпись ------------------- */
   QString txt = QString::number(std::stoi(m_pin->GetInitState(), 0, 2), 16)
                     .toUpper();

   /*   запоминаем ТМ и переводим сцену → экран, чтобы узнать Y-коорд.  */
   p->save();
   const QTransform viewTm = p->worldTransform();
   const qreal baseY = viewTm.map(QPointF(0, yNeg)).y(); // низ «0»-уровня

   /*   полностью обнуляем трансформацию – теперь (0,0) = левый-верх окна  */
   p->resetTransform();

   QFont f("Monospace");
   f.setPixelSize(15); // всегда 15-px
   p->setFont(f);
   p->setPen(Qt::white);

   const qreal margin = 4.0; // отступ от левого края окна
   p->drawText(QPointF(margin, baseY - 5), txt);

   p->restore();
}
