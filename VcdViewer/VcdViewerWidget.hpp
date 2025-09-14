#pragma once
/*--------------------------------------------------------------------------
 *  VcdViewerWidget
 *  ----------------
 *  Главный виджет-контейнер VCD-просмотрщика.
 *------------------------------------------------------------------------*/

#include <QWidget>
#include <QVector>
#include <optional>
#include <atomic>
#include <unordered_map>

#include "Include/VcdStructs.hpp" // PinDescriptionPtr
#include "VcdAsyncReader.hpp"
#include "WaveformView.hpp"
#include "PinTableModel.hpp"
#include "SignalsTreeModel.hpp"
#include "TreeModulesModel.hpp"

class QTreeView;
class QTableView;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollBar;
class QSplitter;

class SignalTreeView;
class SnapScrollBar;

/*------------------------------------------------------------------*/
class VcdViewerWidget : public QWidget
{
   Q_OBJECT
public:
   explicit VcdViewerWidget(QWidget *parent = nullptr);

signals:
   void AskForReadFile(QString filePath);

   void MarkerPositionUpdated(quint64 pos);
   void CursorPositionUpdated(quint64 pos);

   void AskForReportPreparation(std::shared_ptr<vcd::Handle> handle);
   void ReportReady(const QString& report);

public slots:
   void FixZoom();
   void UnloadPreviousData();
   void UpdateMarkerPosition(quint64 pos);
   void UpdateCursorPosition(quint64 pos);

private slots:
   /* кнопки масштабирования */
   void OnZoomInClicked();
   void OnZoomOutClicked();
   void OnZoomResetClicked();

   /* операции со списком сигналов */
   void OnAppendSignals();
   void OnReplaceSignals();
   void OnInsertSignals();

   /* дерево модулей → таблица пинов */
   void OnModuleClicked(std::shared_ptr<vcd::Module> module);

   /* диапазон «From-To» */
   void OnApplyRangeClicked();

   /* асинхронный ридер */
   void
   OnReadFileReady(
       std::shared_ptr<vcd::Handle> handle);

   void OnReadFileError(
       const QString &msg);

   void OnPrepareReport(std::shared_ptr<vcd::Handle> handle);

   /* обновление значений сигналов (бывшая лямбда) */
   void
   OnSignalsUpdated(
       const QVector<EmmitedSignalDescription> &);

private:
   /* вспомогательные методы */
   void
   BuildUi();

   void
   MakeConnections();

   void
   UpdateMarkerAndCursorLabel();

   std::vector<vcd::PinDescriptionPtr>
   SelectedPins() const;

   std::optional<quint64>
   ParseTime(
       const QString &text) const;

   /* ---------- данные экземпляра ---------- */
   VcdAsyncFileReader *m_reader{nullptr};

   QTreeView *m_modulesView{nullptr};
   QTableView *m_pinsView{nullptr};
   SignalTreeView *m_signalTreeView{nullptr};
   WaveformView *m_waveView{nullptr};

   ModuleTreeModel *m_moduleModel{nullptr};
   PinTableModel *m_pinModel{nullptr};
   SignalTreeModel *m_signalModel{nullptr};

   /* кнопки держим как поля для connect-ов */
   QPushButton *m_btnZoomIn{nullptr};
   QPushButton *m_btnZoomOut{nullptr};
   QPushButton *m_btnZoomReset{nullptr};
   QPushButton *m_btnInsert{nullptr};
   QPushButton *m_btnReplace{nullptr};
   QPushButton *m_btnAppend{nullptr};
   QPushButton *m_btnApplyRange{nullptr};

   QLabel *m_lblCursorMarker{nullptr};
   QLineEdit *m_editFrom{nullptr};
   QLineEdit *m_editTo{nullptr};

   std::atomic_bool m_prevZoomOut{false};
   std::optional<quint64> m_markerPos;
   std::optional<quint64> m_cursorPos;

   std::string m_timeScale; ///< «1ns» -> «ns»
   uint64_t m_maxTs;

   static const std::unordered_map<std::string, double> s_scale;
};
