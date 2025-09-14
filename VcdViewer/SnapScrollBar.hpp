#pragma once

#include <QScopedValueRollback>
#include <QScrollBar>

class SnapScrollBar : public QScrollBar
{
   Q_OBJECT
public:
   explicit SnapScrollBar(Qt::Orientation o,
                          QWidget *parent = nullptr)
       : QScrollBar(o, parent)
   {
   }

   void
   setSnapStep(int s)
   {
      m_step = s;
      setSingleStep(s);
   }

signals:
   void snapped(int); // значение ПОСЛЕ «прилипания»

protected:
   void
   sliderChange(SliderChange change) override
   {
      if (change == SliderValueChange && m_step > 0)
      {
         int raw = value();
         int snappedVal =
             ((raw + m_step / 2) / m_step) * m_step;

         if (snappedVal != raw)
         {
            /* блокируем ТОЛЬКО собственные сигналы,
               чтобы наружу не шёл  raw */
            QSignalBlocker guard(this);
            QScrollBar::setValue(snappedVal);
         }

         emit snapped(snappedVal);
         return;
      }

      QScrollBar::sliderChange(change);
   }

private:
   int m_step = 0;
};
