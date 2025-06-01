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
   SimpleWaveItem(const std::shared_ptr<newVcd::Handle>& h,
                  std::shared_ptr<newVcd::SimplePinDescription> p,
                  int yOffset,
                  std::optional<std::size_t> idx = std::nullopt,
                  QGraphicsItem* parent = nullptr);

   QRectF
   boundingRect() const override;

   void
   paint(QPainter* p,
         const QStyleOptionGraphicsItem* opt,
         QWidget*) override;

 private:
   std::shared_ptr<newVcd::Handle> handle;
   std::shared_ptr<newVcd::SimplePinDescription> pin;
   QPainterPath precalcedPath;

   std::optional<std::size_t> idx;

   std::vector<QPainterPath> precalcedZPath;
   std::vector<QRect> precalcedXRectangles;

   // строит полный путь для всей последовательности
   void
   precalcFullPath();

   QString
   GetPinValueAtTimestamp(
      std::size_t index, uint64_t timestamp);

   // бинарный поиск t, чтобы path.pointAtPercent(t).x() ≈ xTarget
   static qreal
   findT(const QPainterPath& path, qreal xTarget, int iters = 30);

   // вырезает из path кусок между x0 и x1 (аппроксимация полилинией с sampleSteps точками)
   static QPainterPath
   subPathByX(const QPainterPath& path,
              qreal x0, qreal x1,
              int sampleSteps = 50);
};

class ParamWaveItem final : public QObject, public QGraphicsItem
{
   Q_OBJECT
 public:
   ParamWaveItem(
      const std::shared_ptr<newVcd::Handle>& h,
      std::shared_ptr<newVcd::ParamPinDescription> p,
      int yOffset,
      QGraphicsItem* parent = nullptr);

   void
   paint(
      QPainter* p,
      const QStyleOptionGraphicsItem* opt,
      QWidget*) override;

   QRectF
   boundingRect() const override;

 private:
   std::shared_ptr<newVcd::Handle> handle;
   std::shared_ptr<newVcd::ParamPinDescription> pin;
   QGraphicsSimpleTextItem* label = nullptr;
};

class MultipleWaveItem final : public QObject, public QGraphicsItem
{
   Q_OBJECT
 public:
   MultipleWaveItem(
      const std::shared_ptr<newVcd::Handle>& h,
      std::shared_ptr<newVcd::MultiplePinDescription> p,
      int yOffset,
      QGraphicsItem* parent = nullptr);

   void
   paint(
      QPainter* p,
      const QStyleOptionGraphicsItem* opt,
      QWidget*) override;

   QRectF
   boundingRect() const override;

 public slots:
   void
   SetScaleCoeff(double newCoeff)
   {
      scaleCoeff = newCoeff;
   }

   void
   SetExpanded(bool isExpanded);

 private:
   void
   PreparePaths();

   QString
   StateAt(
      uint64_t ts);

   // void
   // AddPoly(
   //    QPainterPath
   //)

   void
   PrepareSubItems();

   std::shared_ptr<newVcd::Handle> handle;
   newVcd::PinDescriptionPtr pin;

   std::pair<QPainterPath, QPainterPath> precalcedLines;

   std::vector<QPainterPath> precalcedZPath;
   std::vector<QRect> precalcedXRectangles;
   std::vector<QGraphicsSimpleTextItem*> textItems;

   double scaleCoeff = 1;

   bool isExpanded = false;
   double prepared = false;
   std::vector<SimpleWaveItem*> subWaveItems;
};

#endif //!__WAVE_ITEMS_HPP__