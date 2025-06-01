#include "WaveItems.hpp"

#include "Include/VcdStructs.hpp"

#include <QDebug>
#include <iostream>

MultipleWaveItem::MultipleWaveItem(
   const std::shared_ptr<newVcd::Handle>& h,
   std::shared_ptr<newVcd::MultiplePinDescription> p,
   int yOffset,
   QGraphicsItem* parent)
   : QGraphicsItem(parent)
   , handle(h)
   , pin(p)
{
   setPos(0, yOffset);
   // setCacheMode(QGraphicsItem::CacheMode::ItemCoordinateCache);

   PreparePaths();
}

void
MultipleWaveItem::paint(
   QPainter* p,
   const QStyleOptionGraphicsItem* opt,
   QWidget*)
{
   /*
   const uint64_t leftBound = opt->exposedRect.left();
   const uint64_t rightBound = opt->exposedRect.right();
   const uint64_t width = rightBound - leftBound;

   const double xStep = WAVEFORM_HEIGHT * scaleCoeff / 10;

   const int yUpper = SPACING;
   const int yLower = WAVEFORM_HEIGHT;
   const int yMid = WAVEFORM_HEIGHT / 2;
   const int yZ = WAVEFORM_HEIGHT / 2;
   const int yDiff = yUpper - yLower;

   precalcedLines.first.clear();
   precalcedLines.second.clear();

   QPainterPath& upperLine = precalcedLines.first;
   QPainterPath& lowerLine = precalcedLines.second;

   auto itL = handle->timeStamps.lower_bound(leftBound);
   auto itR = handle->timeStamps.upper_bound(rightBound);

   upperLine.moveTo(*itL, yMid);
   lowerLine.moveTo(*itL, yMid);

   QString prev = StateAt(0);

   for (std::size_t i = 0; i < textItems.size(); ++i)
   {
      delete textItems[i];
   }
   textItems.clear();

   QString prevValue = GetPinValueAtTimestamp(pin, *itL);
   std::pair<QPainterPath, QPainterPath> xLines;
   QPainterPath& upperXLine = xLines.first;
   QPainterPath& lowerXline = xLines.second;
   QPainterPath zLine;

   auto drawAngles = [xStep, yLower, yUpper, yMid](
                        QPainterPath* upperLine,
                        QPainterPath* lowerLine,
                        uint64_t x,
                        bool right)
   {
      if (!right)
      {
         if (upperLine)
         {
            upperLine->lineTo(x + xStep, yUpper);
         }
         if (lowerLine)
         {
            lowerLine->lineTo(x + xStep, yLower);
         }
      }
      else
      {
         if (upperLine)
         {
            upperLine->lineTo(x - xStep, yUpper);
            upperLine->lineTo(x, yMid);
         }
         if (lowerLine)
         {
            lowerLine->lineTo(x - xStep, yLower);
            lowerLine->lineTo(x, yMid);
         }
      }
   };

   if (prevValue == "0")
   {
      lowerLine.moveTo(*itL, yMid);
      drawAngles(nullptr, &lowerLine, *itL, false);
   }
   else if (prevValue == "z")
   {
      zLine.moveTo(*itL, yMid);
   }
   else if (prevValue == "x")
   {
      upperXLine.moveTo(*itL, yMid);
      lowerXline.moveTo(*itL, yMid);
      drawAngles(&upperXLine, &lowerXline, *itL, false);
   }
   else
   {
      upperLine.moveTo(*itL, yMid);
      lowerLine.moveTo(*itL, yMid);
      drawAngles(&upperLine, &lowerLine, *itL, false);
   }

   for (auto it = std::next(itL); it != itR; ++it)
   {
      QString currentValue = GetPinValueAtTimestamp(pin, *it);
      qWarning() << currentValue;

      if (currentValue == prevValue)
      {
         continue;
      }

      if (prevValue == "0")
      {
         drawAngles(nullptr, &lowerLine, *it, true);
      }
      else if (prevValue == "z")
      {
         zLine.lineTo(*it, yMid);
      }
      else if (prevValue == "x")
      {
         drawAngles(&upperXLine, &lowerXline, *it, true);
      }
      else
      {
         drawAngles(&upperLine, &lowerLine, *it, true);
      }

      if (currentValue == "0")
      {
         lowerLine.moveTo(*it, yMid);
         drawAngles(nullptr, &lowerLine, *it, false);
      }
      else if (currentValue == "z")
      {
         zLine.moveTo(*it, yMid);
      }
      else if (currentValue == "x")
      {
         upperXLine.moveTo(*it, yMid);
         lowerXline.moveTo(*it, yMid);
         drawAngles(&upperXLine, &lowerXline, *it, false);
      }
      else
      {
         upperLine.moveTo(*it, yMid);
         lowerLine.moveTo(*it, yMid);
         drawAngles(&upperLine, &lowerLine, *it, false);
      }
      prevValue = currentValue;
   }

   {
      QPen pen(Qt::green, 1);
      pen.setCosmetic(true);
      p->setPen(pen);
      p->drawPath(upperLine);
      p->drawPath(lowerLine);
   }

   {
      QPen pen(Qt::red, 1);
      pen.setCosmetic(true);
      p->setPen(pen);
      p->drawPath(upperXLine);
      p->drawPath(lowerXline);
   }

   {
      QPen pen(Qt::yellow, 1);
      pen.setCosmetic(true);
      p->setPen(pen);
      p->drawPath(zLine);
   }
   */
}

void
MultipleWaveItem::PrepareSubItems()
{
   /*
   if (prepared)
   {
      return;
   }

   // высота первой подволны сразу под основной
   int y = WAVEFORM_HEIGHT + SPACING;

   for (std::size_t bit = 0; bit < pin->size; ++bit)
   {
      auto* sub = new SimpleWaveItem(handle, pin, y, bit, this); // <-- parent = this
      sub->setVisible(isExpanded);                               // показываем/прячем при сворачивании
      subWaveItems.push_back(sub);

      y += WAVEFORM_HEIGHT + SPACING; // следующая строка
   }
   prepared = true;
   */
}

void
MultipleWaveItem::SetExpanded(bool isExpanded)
{
   this->isExpanded = isExpanded;
   PrepareSubItems();
   for (auto& it : subWaveItems)
   {
      it->setVisible(this->isExpanded);
   }
}

QRectF
MultipleWaveItem::boundingRect() const
{
   /*
   qreal summaryHeight = WAVEFORM_HEIGHT + SPACING;
   if (isExpanded)
   {
      summaryHeight += (SPACING + WAVEFORM_HEIGHT) * subWaveItems.size();
   }

   return {
      0.0,
      0.0,
      qreal(*handle->timeStamps.rbegin()),
      summaryHeight};
      */
   return {
      0.0,
      0.0,
      0.0,
      0.0};
}

QString
MultipleWaveItem::StateAt(
   uint64_t ts)
{
   /*
   auto rg = handle->timeStamp2ValueMap.equal_range(ts);
   for (auto i = rg.first; i != rg.second; ++i)
   {
      if (i->second.pPinDescription->alias == pin->alias)
      {
         std::string ret = i->second.state;
         return QString::fromStdString(ret);
      }
   }
   std::string ret = handle->pinAlias2InitStateMap[pin->alias].state;
   return QString::fromStdString(ret);*/
   return "";
}

void
MultipleWaveItem::PreparePaths()
{
}
