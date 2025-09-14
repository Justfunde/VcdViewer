#include "WaveItems.hpp"

#include <QDebug>
#include <algorithm>
#include <cmath>

namespace
{
   QString binToHex(QString bin)
   {
      bool ok = false;
      quint64 v = bin.toULongLong(&ok, /*base=*/2);
      if (!ok)
         return QStringLiteral("+");

      return QString::number(v, 16).toUpper();
   }

   /* классификация bus-строки */
   char classifyBus(QString v)
   {
      if (v.isEmpty())
         return 'd';
      if (std::all_of(v.begin(), v.end(),
                      [](QChar c)
                      { return c == u'z' || c == u'Z'; }))
         return 'z';
      if (std::any_of(v.begin(), v.end(),
                      [](QChar c)
                      { return c == u'x' || c == u'X'; }))
         return 'x';
      return 'd';
   }
} // unnamed namespace

/* ===== ctor ===== */
MultipleWaveItem::MultipleWaveItem(
    const std::shared_ptr<vcd::Handle> &h,
    std::shared_ptr<vcd::BusPinDescription> p,
    int yOffset,
    double scale,
    QGraphicsItem *parent)
    : QGraphicsItem(parent), m_scaleCoeff(scale), m_handle(h), m_pin(std::move(p))
{
   setPos(0, yOffset);
   setCacheMode(DeviceCoordinateCache);
   PreparePaths();
}

/* =========================================================================
 *  PreparePaths  (перестраивается при зуме — scaleCoeff)
 * ========================================================================= */
void MultipleWaveItem::PreparePaths()
{
   /* 0. очистка ------------------------------------------------------- */
   m_pathDataUpper = m_pathDataLower = QPainterPath();
   m_pathXUpper = m_pathXLower = QPainterPath();
   m_pathZ = QPainterPath();
   m_labels.clear();

   /* 1. геометрия по Y -------------------------------------------------- */
   const int yU = SPACING;         // верх
   const int yL = WAVEFORM_HEIGHT; // низ
   const int yM = (yU + yL) / 2;   // середина
   const double xStep = (WAVEFORM_HEIGHT / 10.0) * m_scaleCoeff;

   auto stairs = [=](QPainterPath &up, QPainterPath &lo,
                     double x, bool right)
   {
      if (!right)
      { /* левый угол */
         up.lineTo(x + xStep, yU);
         lo.lineTo(x + xStep, yL);
      }
      else
      { /* правый угол */
         up.lineTo(x - xStep, yU);
         up.lineTo(x, yM);
         lo.lineTo(x - xStep, yL);
         lo.lineTo(x, yM);
      }
   };

   /* 2. инициализация состояний --------------------------------------- */
   const QString init = QString::fromStdString(m_pin->GetInitState());
   const auto &tl = m_pin->GetTimeline();
   auto it = tl.begin();

   char curCls = 'd';
   QString curBits = "0"; // для hex-подписи
   if (!init.isEmpty())
   {
      curBits = init;
      curCls = ::classifyBus(init);
   }
   else if (it != tl.end())
   {
      curBits = QString::fromLatin1(it->value.data(), int(it->value.size()));
      curCls = ::classifyBus(curBits);
   }

   quint64 segBeg = 0; // левая граница текущего ромба

   /* 3. helpers open/close -------------------------------------------- */
   auto open = [&](char cls, quint64 x, const QString &raw)
   {
      if (cls == 'd' || cls == 'x')
      { // и «data», и «x» имеют подписи
         segBeg = x;
         curBits = raw; // raw игнорируется для «x»
      }

      switch (cls)
      {
      case 'd':
         m_pathDataUpper.moveTo(x, yM);
         m_pathDataLower.moveTo(x, yM);
         stairs(m_pathDataUpper, m_pathDataLower, x, false);
         break;
      case 'x':
         m_pathXUpper.moveTo(x, yM);
         m_pathXLower.moveTo(x, yM);
         stairs(m_pathXUpper, m_pathXLower, x, false);
         break;
      case 'z':
         m_pathZ.moveTo(x, yM);
         break;
      }
   };

   auto close = [&](char cls, quint64 x)
   {
      switch (cls)
      {
      case 'd':
         stairs(m_pathDataUpper, m_pathDataLower, x, true);
         if (x > segBeg + 2 * xStep) // место под подпись
            m_labels.push_back({static_cast<uint64_t>(segBeg + xStep),
                                static_cast<uint64_t>(x - xStep),
                                binToHex(QString(curBits))});
         break;

      case 'x':
         stairs(m_pathXUpper, m_pathXLower, x, true);
         if (x > segBeg + 2 * xStep)
            m_labels.push_back({static_cast<uint64_t>(segBeg + xStep),
                                static_cast<uint64_t>(x - xStep),
                                QStringLiteral("x")});
         break;

      case 'z':
         m_pathZ.lineTo(x, yM);
         break;
      }
   };

   /* 4. построение ломаных -------------------------------------------- */
   open(curCls, 0, curBits);

   for (; it != tl.end(); ++it)
   {
      quint64 ts = it->timestamp;
      QString raw = QString::fromLatin1(it->value.data(), int(it->value.size()));
      char nxt = ::classifyBus(raw);

      close(curCls, ts);
      open(nxt, ts, raw);
      curCls = nxt;
   }
   close(curCls, m_handle->GetMaxTs());
}

/* ======================================================================
 *  MultipleWaveItem :: paint()
 *  ────────────────────────────────────────────────────────────────────
 *  • выводит «ломаные» (зелёная, красная, жёлтая)
 *  • поверх них — подписи шины (`hex`-текст | «+» | пусто)
 *    - шрифт фиксированного размера, не масштабируется вместе с вью
 * ===================================================================== */
void MultipleWaveItem::paint(QPainter *p,
                             const QStyleOptionGraphicsItem *,
                             QWidget *)
{
   /* ── линии ──────────────────────────────────────────────────────── */
   p->setRenderHint(QPainter::Antialiasing);

   QPen pen(Qt::green, 1);
   pen.setCosmetic(true);
   p->setPen(pen);
   p->drawPath(m_pathDataUpper);
   p->drawPath(m_pathDataLower);

   pen.setColor(Qt::red);
   p->setPen(pen);
   p->drawPath(m_pathXUpper);
   p->drawPath(m_pathXLower);

   pen.setColor(Qt::yellow);
   p->setPen(pen);
   p->drawPath(m_pathZ);

   /* ── подписи ─────────────────────────────────────────────────────── */
   QFont fixedFont("Monospace");
   fixedFont.setPixelSize(12); // постоянный размер
   QFontMetrics fm(fixedFont);
   const int plusPx = fm.horizontalAdvance(u'+');

   const double sx = p->worldTransform().m11(); // масштаб по X

   struct DevTxt
   {
      QPointF pos;
      QString txt;
   };
   std::vector<DevTxt> todo;
   todo.reserve(m_labels.size());

   for (const BusLabel &lbl : m_labels)
   {
      double wPx = (lbl.x1 - lbl.x0) * sx;
      if (wPx <= 0)
         continue;

      int fullPx = fm.horizontalAdvance(lbl.text);
      QString draw;
      if (fullPx <= wPx)
         draw = lbl.text;
      else if (plusPx <= wPx)
         draw = QStringLiteral("+");
      else
         continue;

      double txtWScene = fm.horizontalAdvance(draw) / sx;
      double xScene = lbl.x0 + ((lbl.x1 - lbl.x0) - txtWScene) / 2.0;
      double yScene = (SPACING + WAVEFORM_HEIGHT) / 2.0 + fm.ascent() * 0.4;

      todo.push_back({p->worldTransform().map(QPointF(xScene, yScene)),
                      std::move(draw)});
   }

   /* выводим без трансформации (в пикселях) */
   p->save();
   p->resetTransform();
   p->setFont(fixedFont);
   p->setPen(Qt::white);
   for (const auto &d : todo)
      p->drawText(d.pos, d.txt);
   p->restore();
}

/* ===== boundingRect ===== */
QRectF MultipleWaveItem::boundingRect() const
{
   qreal h = WAVEFORM_HEIGHT + SPACING;
   if (m_isExpanded)
      h += (WAVEFORM_HEIGHT + SPACING) * m_.size();

   return {0.0, 0.0,
           qreal(m_handle->GetMaxTs()),
           h};
}

/* ===== раскрытие/сворачивание ===== */
void MultipleWaveItem::PrepareSubItems()
{
   if (m_prepared)
      return;
   m_prepared = true;

   const auto bits = m_pin->GetBitDepth();
   std::size_t nBits = bits.first - bits.second + 1;
   if (nBits == 0)
      return;

   int y = WAVEFORM_HEIGHT + SPACING;
   for (std::size_t b = 0; b < nBits; ++b)
   {
      // если в парсере заполняется pin->subPins(), используйте его.
      if (b < m_pin->GetSubPins().size())
      {
         auto subPin = m_pin->GetSubPins()[m_pin->GetSubPins().size() - b - 1];
         auto *item = new SimpleWaveItem(m_handle, subPin, y, m_pin->GetSubPins().size() - b - 1, this);
         item->setVisible(m_isExpanded);
         m_.push_back(item);
         y += WAVEFORM_HEIGHT + SPACING;
      }
   }
}

void MultipleWaveItem::SetExpanded(bool on)
{
   m_isExpanded = on;
   PrepareSubItems();
   for (auto *w : m_)
      w->setVisible(m_isExpanded);
}
