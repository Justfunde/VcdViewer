/************************  WaveformView.hpp  ************************/
#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <QGraphicsView>

#include "Include/VcdStructs.hpp"
#include "WaveformItem/Parameters.hpp"
#include "WaveformItem/WaveItems.hpp"
#include "SnapScrollBar.hpp"

class QGraphicsScene;
class QGraphicsLineItem;
class QScrollBar;
class QMouseEvent;
class QWheelEvent;
class QResizeEvent;
class DumpoffItem;

/**
 * @brief Вид для отображения временных диаграмм (waveforms).
 *
 * Графический компонент, позволяющий масштабировать, скроллировать и
 * выбирать временной срез курсором.
 */
class WaveformView final : public QGraphicsView
{
  Q_OBJECT

public:
  explicit WaveformView(QWidget *parent = nullptr);

  bool HasScrollBar() const { return horizontalScrollBar()->isVisible(); }
  uint64_t GetMaxTimestamp() const { return m_handle ? m_handle->GetMaxTs() : 0; }
  QGraphicsView *GetScaleView(); ///< вспомогательный QGraphicsView с линейкой
  QScrollBar *GetVerticalScrollBar() const { return verticalScrollBar(); }

signals:
  void SelectedTimestampChange(uint64_t ts);
  void ScaleCoeffChanged(double coeff);
  void PointerPositionChanged(uint64_t x);

public slots:
  void zoomToRange(uint64_t x0, uint64_t x1);
  void ZoomIn();
  bool ZoomOut();
  void SetInitialScale();

  void SetHandle(std::shared_ptr<vcd::Handle> newHandle);
  void OnItemExpandedOrCollapsed(vcd::PinDescriptionPtr pin, bool isExpanded);
  void UpdateSignals(std::vector<vcd::PinDescriptionPtr> newSignals);
  void UpdateScaleViewWidth();
  void DrawScaleLine(bool reset = false); // НЕ ТРОГАТЬ!!!

protected:
  /* events */
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

private: /* helpers – исключительно для внутреннего порядка */
  void createScenes();
  void configureScaleView();
  void setupCursor();
  void initScrollSync();
  void updateCursorGeometry();
  void clearScaleLines();

  void ZoomX(int level);
  double CalcScaleCoeff() const;
  qreal CalculateDevicePixelRatio() const;

private:                                     /* data */
  QGraphicsScene *m_scene = nullptr;         ///< основная сцена
  QGraphicsScene *m_scaleScene = nullptr;    ///< сцена шкалы времени
  QGraphicsView *m_scaleView = nullptr;      ///< отдельный view для шкалы
  QGraphicsLineItem *m_cursorLine = nullptr; ///< вертикальный курсор

  std::shared_ptr<vcd::Handle> m_handle;
  std::vector<vcd::PinDescriptionPtr> m_signals;
  std::unordered_map<vcd::PinDescriptionPtr, bool> m_expandedMap;
  std::unordered_map<std::string, QGraphicsItem *> m_aliasItemMap;
  std::vector<std::string> m_aliasOrder;
  std::vector<QGraphicsLineItem *> m_lineItems; // «фоновые» синие линии

  DumpoffItem *m_dumpoffItem = nullptr;

  int m_currentZoomLevel = 0;
  int m_minZoomLevel = 0;
  qreal m_dpr = 1.0;      ///< device-pixel-ratio
  qint64 m_cursorPos = 0; ///< x-координата курсора в сцене
  double m_currentScaleValue = 0.0;
};
