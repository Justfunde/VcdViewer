#ifndef __WAVE_ITEMS_HPP__
#define __WAVE_ITEMS_HPP__

#include <memory>

#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

#include "Include/VcdStructs.hpp"
#include "Parameters.hpp"

class SimpleWaveItem final : public QObject, public QGraphicsItem
{
   Q_OBJECT
public:
   SimpleWaveItem(const std::shared_ptr<vcd::Handle> &h,
                  std::shared_ptr<vcd::SimplePinDescription> p,
                  int yOffset,
                  std::optional<std::size_t> idx = std::nullopt,
                  QGraphicsItem *parent = nullptr);

   QRectF
   boundingRect() const override;

   void
   paint(QPainter *p,
         const QStyleOptionGraphicsItem *opt,
         QWidget *) override;

private:
   // строит полный путь для всей последовательности
   void
   PrecalcFullPath();

   QString
   GetPinValueAtTimestamp(
       std::size_t index, uint64_t timestamp);

private:
   std::shared_ptr<vcd::Handle> m_handle;
   std::shared_ptr<vcd::SimplePinDescription> m_pin;
   std::optional<std::size_t> m_idx;

   QPainterPath m_precalcedPath;
   std::vector<QPainterPath> m_precalcedZPath;
   std::vector<QRect> m_precalcedXRectangles;
};

class ParamWaveItem final : public QObject, public QGraphicsItem
{
   Q_OBJECT
public:
   ParamWaveItem(
       const std::shared_ptr<vcd::Handle> &h,
       std::shared_ptr<vcd::ParamPinDescription> p,
       int yOffset,
       QGraphicsItem *parent = nullptr);

   void
   paint(
       QPainter *p,
       const QStyleOptionGraphicsItem *opt,
       QWidget *) override;

   QRectF
   boundingRect() const override;

private:
   std::shared_ptr<vcd::Handle> m_handle;
   std::shared_ptr<vcd::ParamPinDescription> m_pin;
   QGraphicsSimpleTextItem *m_label = nullptr;
};

class MultipleWaveItem final : public QObject, public QGraphicsItem
{
   Q_OBJECT
public:
   MultipleWaveItem(const std::shared_ptr<vcd::Handle> &h,
                    std::shared_ptr<vcd::BusPinDescription> p,
                    int yOffset,
                    double scale,
                    QGraphicsItem *parent = nullptr);

   /* ───────────── QGraphicsItem ───────────── */
   QRectF boundingRect() const override;
   void paint(QPainter *,
              const QStyleOptionGraphicsItem *,
              QWidget *) override;

public slots:
   void SetScaleCoeff(double k)
   {
      m_scaleCoeff = k;
      PreparePaths(); // перестраиваем с новым xStep
      update();       // запрос перерисовки
   }
   void SetExpanded(bool on);

private:
   /* ───────────── helpers ───────────── */
   void PreparePaths();
   void PrepareSubItems(); // создаёт SimpleWaveItem’ы для каждого бита

   /** классифицирует строку шины:
       'd' = данные, 'z' = z-состояние, 'x' = неопред. */
   static char classifyBus(const std::string_view &v);

private:
   std::shared_ptr<vcd::Handle> m_handle;
   std::shared_ptr<vcd::BusPinDescription> m_pin;

   /* предварительно рассчитанные пути --------------- */
   QPainterPath m_pathDataUpper, m_pathDataLower; // «нормальные» значения
   QPainterPath m_pathXUpper, m_pathXLower;       // «х»-состояния
   QPainterPath m_pathZ;                          // «z»

   /* подпути-подпины */
   std::vector<SimpleWaveItem *> m_;

   /* misc */
   double m_scaleCoeff = 1.0;
   bool m_isExpanded = false;
   bool m_prepared = false;

   struct BusLabel
   {
      uint64_t x0;  // левая граница ромба   (xStep уже «съеден»)
      uint64_t x1;  // правая граница ромба
      QString text; // готовая строка для вывода
   };

   std::vector<BusLabel> m_labels; // + объявление в private-секции
};

class DumpoffItem final : public QGraphicsItem
{
public:
   DumpoffItem(const std::vector<std::pair<uint64_t, uint64_t>> ranges,
               const std::shared_ptr<vcd::Handle> &h,
               QGraphicsItem *parent = nullptr)
       : QGraphicsItem(parent), m_ranges(ranges), m_handle(h)
   {
   }

   QRectF boundingRect() const override;
   /* ───────────── QGraphicsItem ───────────── */
   void paint(QPainter *,
              const QStyleOptionGraphicsItem *,
              QWidget *) override;

   void setHeight(qreal height)
   {
      this->m_height = height;
   }

private:
   std::vector<std::pair<uint64_t, uint64_t>> m_ranges;
   std::shared_ptr<vcd::Handle> m_handle;
   qreal m_height = 0;
};

#endif //!__WAVE_ITEMS_HPP__