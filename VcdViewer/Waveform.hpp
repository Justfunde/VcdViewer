#pragma once
#include <QDebug>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOptionGraphicsItem> //  ← нужен для QStyleOptionGraphicsItem
#include <QVBoxLayout>
#include <algorithm>
#include <atomic>
#include <cctype> // для std::isdigit
#include <cmath>
#include <memory>
#include <vector>

#include "Include/VcdStructs.hpp"
#include "WaveformItem/Parameters.hpp"
#include "WaveformItem/WaveItems.hpp"

#include "SnapScrollBar.hpp"

/**
 * @brief Класс для отображения временных диаграмм (waveforms) на основе данных VCD.
 *
 * Данный класс наследуется от QGraphicsView и предназначен для визуализации временных диаграмм различных сигналов.
 * Он поддерживает масштабирование, вертикальный и горизонтальный скролл, а также выбор определённого временного среза с помощью курсора.
 */
class WaveformView : public QGraphicsView
{
   Q_OBJECT

 public:
   /**
    * @brief Конструктор класса WaveformView.
    * @param parent Родительский виджет.
    */
   explicit WaveformView(QWidget* parent = nullptr);

   bool
   HasScrollBar() const
   {
      return horizontalScrollBar()->isVisible();
   }

 public:
 signals:
   /**
    * @brief Сигнал, испускаемый при изменении выбранной временной метки (timestamp).
    * @param ts Выбранный временной срез.
    */
   void
   SelectedTimestampChange(uint64_t ts);

   void
   ScaleCoeffChanged(double);

 protected:
   /**
    * @brief Обработчик события нажатия мыши.
    *
    * Переопределяет метод QGraphicsView для установки курсора на выбранное положение по X и вызова сигнала о смене временной метки.
    * @param event Событие нажатия кнопки мыши.
    */
   void mousePressEvent(QMouseEvent* event) override;

   /**
    * @brief Обработчик события прокрутки колёсика мыши.
    *
    * Переопределяется для горизонтального скролла по временной шкале.
    * @param event Событие колёсика мыши.
    */
   void
   wheelEvent(QWheelEvent* event) override;

   /**
    * @brief Обработчик события изменения размера виджета.
    *
    * Переопределяется для обновления ширины шкалы и перерисовки сигналов.
    * @param event Событие изменения размера.
    */
   void
   resizeEvent(QResizeEvent* event) override;

 protected slots:

 public slots:
   /**
    * @brief Увеличить масштаб по оси X.
    */
   void
   ZoomIn();

   /**
    * @brief Установить начальный масштаб диаграммы.
    *
    * Подбирает масштаб, чтобы вся временная шкала умещалась по ширине видимого пространства.
    */
   void
   SetInitialScale();

   /**
    * @brief Уменьшить масштаб по оси X.
    */
   bool ZoomOut();

   /**
    * @brief Установить новый дескриптор (handle) с данными VCD.
    * @param newHandle Новый дескриптор с информацией о временных метках и сигналах.
    */
   void
   SetHandle(
      std::shared_ptr<newVcd::Handle> newHandle);

   /**
    * @brief Обработчик события раскрытия или сворачивания многоразрядного сигнала.
    *
    * Если сигнал является многобитным, при раскрытии будут показаны отдельные биты.
    * @param pin Указатель на описание сигнала.
    * @param isExpanded Флаг, указывающий, раскрыт сигнал или свернут.
    */
   void
   OnItemExpandedOrCollapsed(newVcd::PinDescriptionPtr pin, bool isExpanded);

   /**
    * @brief Обновить отображаемые сигналы.
    * @param newSignals Новый набор сигналов для отображения.
    */
   void
   UpdateSignals(std::vector<newVcd::PinDescriptionPtr> newSignals);

   /**
    * @brief Обновить ширину представления шкалы (ScaleView).
    *
    * Используется при изменении видимого размера и вертикальной прокрутке.
    */
   void
   UpdateScaleViewWidth();

   /**
    * @brief Отрисовка временной шкалы.
    * @param reset Если true, то шкала перерисовывается с нуля.
    */
   void
   DrawScaleLine(bool reset = false);

 public:
   /**
    * @brief Получить указатель на представление временной шкалы.
    * @return Указатель на QGraphicsView шкалы.
    */
   QGraphicsView*
   GetScaleView();

   /**
    * @brief Получить указатель на вертикальный скроллбар.
    * @return Указатель на QScrollBar.
    */
   QScrollBar*
   GetVerticalScrollBar() const;

 private:
   double CalcScaleCoeff() const;

   qreal
   CalculateDevicePixelRatio() const;

   /**
    * @brief Масштабировать представление по оси X до указанного уровня.
    * @param level Новый уровень масштаба.
    */
   void
   ZoomX(int level);

   void
   DrawDumpoffIntervals(
      QGraphicsScene* scene,
      const std::vector<std::pair<uint64_t, uint64_t>>& dumpoffIntervals);

 private:
   QGraphicsScene* m_scene;                                           ///< Сцена с диаграммами сигналов.
   QGraphicsScene* m_scaleScene;                                      ///< Сцена с временной шкалой.
   QGraphicsView* m_scaleView;                                        ///< Представление для временной шкалы.
   QGraphicsLineItem* m_cursorLine;                                   ///< Вертикальная линия курсора для выбранного timestamp.
   std::shared_ptr<newVcd::Handle> m_handle;                          ///< Объект с данными VCD (временные метки, сигналы, значения).
   std::vector<newVcd::PinDescriptionPtr> m_signals;                  ///< Отображаемые сигналы.
   int m_currentZoomLevel = 0;                                        ///< Текущий уровень масштабирования.
   int m_minZoomLevel = 0;                                            ///< Минимальный оптимальный уровень зума
   qreal m_dpr = CalculateDevicePixelRatio();                         ///< Масштаб в системе
   int64_t m_cursorPos = 0;                                           ///< Позиция курсора по оси X.
   double m_currentScaleValue = 0;                                    ///< Текущий масштаб по оси X.
   std::unordered_map<newVcd::PinDescriptionPtr, bool> m_expandedMap; ///< Отслеживание раскрытия многоразрядных сигналов.
   std::unordered_map<std::string, QGraphicsItem*> m_aliasItemMap;
   std::vector<std::string> m_aliasOrder;
   std::vector<QGraphicsLineItem*> m_lineItems;
};
