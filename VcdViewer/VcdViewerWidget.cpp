#include "VcdViewerWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTimer>
#include <QTableView>
#include <stack>

#include <cmath>
#include <iostream>
#include <limits>
#include <iostream>

using namespace vcd;

/* коэффициенты перевода единиц ------------------------------------------------*/
const std::unordered_map<std::string, double> VcdViewerWidget::s_scale =
    {
        {"ps", 1.0},
        {"ns", 1e3},
        {"us", 1e6},
        {"ms", 1e9},
        {"s", 1e12}};

/*=============================================================================*/
VcdViewerWidget::VcdViewerWidget(QWidget *parent)
    : QWidget(parent)
{
   BuildUi();
   MakeConnections();
}

/*------------------------- UI ------------------------------------------------*/
void VcdViewerWidget::BuildUi()
{
   setWindowTitle(QStringLiteral("VCD Module and Signal Viewer"));
   resize(1400, 1300);

   /* --- кнопки ------------------------------------------------------ */
   m_btnZoomIn = new QPushButton;
   m_btnZoomIn->setIcon(QIcon(":icons/zoomIn.ico"));
   m_btnZoomOut = new QPushButton;
   m_btnZoomOut->setIcon(QIcon(":icons/zoomOut.ico"));
   m_btnZoomReset = new QPushButton;
   m_btnZoomReset->setIcon(QIcon(":icons/zoomReset.ico"));

   m_btnInsert = new QPushButton(QStringLiteral("Insert"));
   m_btnReplace = new QPushButton(QStringLiteral("Replace"));
   m_btnAppend = new QPushButton(QStringLiteral("Append"));

   m_btnApplyRange = new QPushButton(QStringLiteral("Apply"));
   m_editFrom = new QLineEdit;
   m_editFrom->setPlaceholderText("From");
   m_editTo = new QLineEdit;
   m_editTo->setPlaceholderText("To");

   /* --- основные виды ----------------------------------------------- */
   m_modulesView = new QTreeView(this);
   m_signalTreeView = new SignalTreeView(this);
   m_pinsView = new QTableView(this);
   m_waveView = new WaveformView(this);
   m_waveView->setFrameStyle(QFrame::NoFrame);
   m_waveView->GetScaleView()->setFrameStyle(QFrame::NoFrame);

   m_lblCursorMarker = new QLabel(this);

   /* --- модели ------------------------------------------------------ */
   m_moduleModel = new ModuleTreeModel(this);
   m_signalModel = new SignalTreeModel(this);
   m_pinModel = new PinTableModel(this);

   m_modulesView->setModel(m_moduleModel);
   m_modulesView->setHeaderHidden(true);

   m_signalTreeView->setModel(m_signalModel);

   m_pinsView->setModel(m_pinModel);
   m_pinsView->setSelectionBehavior(QAbstractItemView::SelectRows);
   for (int c = 0; c < m_pinsView->horizontalHeader()->count(); ++c)
      m_pinsView->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);

   /* --- layout кнопок сигналов -------------------------------------- */
   QHBoxLayout *opLayout = new QHBoxLayout;
   opLayout->addWidget(m_btnAppend);
   opLayout->addWidget(m_btnInsert);
   opLayout->addWidget(m_btnReplace);

   /* --- левый столбец ----------------------------------------------- */
   QVBoxLayout *leftLayout = new QVBoxLayout;
   leftLayout->addWidget(m_modulesView);
   leftLayout->addWidget(m_pinsView);
   leftLayout->addLayout(opLayout);

   QWidget *leftWidget = new QWidget(this);
   leftWidget->setLayout(leftLayout);

   /* --- правый столбец ---------------------------------------------- */
   QVBoxLayout *waveLayout = new QVBoxLayout;
   waveLayout->setContentsMargins(0, 0, 0, 0);
   waveLayout->setSpacing(0);
   waveLayout->addWidget(m_waveView->GetScaleView());
   waveLayout->addWidget(m_waveView);

   QHBoxLayout *rightLayout = new QHBoxLayout;
   rightLayout->setContentsMargins(0, 0, 0, 0);
   rightLayout->setSpacing(0);
   rightLayout->addWidget(m_signalTreeView);
   rightLayout->addLayout(waveLayout);
   rightLayout->setStretch(0, 1);
   rightLayout->setStretch(1, 2);

   QWidget *rightWidget = new QWidget(this);
   rightWidget->setLayout(rightLayout);

   /* --- сплиттер ---------------------------------------------------- */
   QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
   splitter->addWidget(leftWidget);
   splitter->addWidget(rightWidget);
   splitter->setStretchFactor(0, 1);
   splitter->setStretchFactor(1, 3);
   leftWidget->setMinimumWidth(200);
   rightWidget->setMinimumWidth(400);

   /* --- верхняя панель --------------------------------------------- */
   QHBoxLayout *rangeLayout = new QHBoxLayout;
   rangeLayout->addWidget(new QLabel("From:", this));
   rangeLayout->addWidget(m_editFrom);
   rangeLayout->addWidget(new QLabel("To:", this));
   rangeLayout->addWidget(m_editTo);
   rangeLayout->addWidget(m_btnApplyRange);

   QHBoxLayout *topLayout = new QHBoxLayout;
   topLayout->addWidget(m_btnZoomIn);
   topLayout->addWidget(m_btnZoomOut);
   topLayout->addWidget(m_btnZoomReset);
   topLayout->addLayout(rangeLayout);
   topLayout->addWidget(m_lblCursorMarker);
   topLayout->addStretch(1);

   /* --- главный макет ---------------------------------------------- */
   QVBoxLayout *mainLayout = new QVBoxLayout(this);
   mainLayout->addLayout(topLayout, 0);
   mainLayout->addWidget(splitter, 1);
}

/*------------------------- connections -------------------------------------*/
void VcdViewerWidget::MakeConnections()
{
   /* ридер */
   m_reader = new VcdAsyncFileReader(this);
   connect(this, &VcdViewerWidget::AskForReadFile,
           m_reader, &VcdAsyncFileReader::ReadFile);
   connect(m_reader, &VcdAsyncFileReader::ReadFileReady,
           this, &VcdViewerWidget::OnReadFileReady);
   connect(m_reader, &VcdAsyncFileReader::ReadFileError,
           this, &VcdViewerWidget::OnReadFileError);

   /* zoom */
   connect(m_btnZoomIn, &QPushButton::clicked, this, &VcdViewerWidget::OnZoomInClicked);
   connect(m_btnZoomOut, &QPushButton::clicked, this, &VcdViewerWidget::OnZoomOutClicked);
   connect(m_btnZoomReset, &QPushButton::clicked, this, &VcdViewerWidget::OnZoomResetClicked);

   /* сигнал-кнопки */
   connect(m_btnAppend, &QPushButton::clicked, this, &VcdViewerWidget::OnAppendSignals);
   connect(m_btnReplace, &QPushButton::clicked, this, &VcdViewerWidget::OnReplaceSignals);
   connect(m_btnInsert, &QPushButton::clicked, this, &VcdViewerWidget::OnInsertSignals);

   /* диапазон */
   connect(m_btnApplyRange, &QPushButton::clicked,
           this, &VcdViewerWidget::OnApplyRangeClicked);

   /* дерево модулей → реакция на клик (вернули) */
   connect(m_modulesView, &QTreeView::clicked,
           m_moduleModel, &ModuleTreeModel::OnItemClicked);
   connect(m_moduleModel, &ModuleTreeModel::ModuleClicked,
           this, &VcdViewerWidget::OnModuleClicked);

   /* сигнал-значения */
   connect(m_pinModel, &PinTableModel::PinClicked,
           m_signalModel, &SignalTreeModel::AppendSignal);

   connect(m_signalModel, &SignalTreeModel::SignalListChanged,
           m_waveView, &WaveformView::UpdateSignals);

   connect(m_pinsView, &QTreeView::doubleClicked, m_pinModel, &PinTableModel::OnItemClicked);
   connect(m_pinModel, &PinTableModel::PinClicked, m_signalModel, &SignalTreeModel::AppendSignal);

   /* курсор и маркер */
   connect(m_waveView, &WaveformView::SelectedTimestampChange,
           m_signalModel, &SignalTreeModel::SetSelectedTimestamp);
   connect(m_waveView, &WaveformView::SelectedTimestampChange,
           this, &VcdViewerWidget::UpdateMarkerPosition);
   connect(m_waveView, &WaveformView::PointerPositionChanged,
           this, &VcdViewerWidget::UpdateCursorPosition);

   connect(m_waveView, &WaveformView::SelectedTimestampChange,
           this, &VcdViewerWidget::MarkerPositionUpdated);
   connect(m_waveView, &WaveformView::PointerPositionChanged,
           this, &VcdViewerWidget::CursorPositionUpdated);

   /* scroll-sync */
   QScrollBar *treeSB = m_signalTreeView->GetVerticalScrollBar();
   QScrollBar *waveSB = m_waveView->GetVerticalScrollBar();
   connect(treeSB, &QScrollBar::valueChanged,
           waveSB, &QScrollBar::setValue);

   SnapScrollBar *snapSB = qobject_cast<SnapScrollBar *>(waveSB);
   if (snapSB != nullptr)
      connect(snapSB, &SnapScrollBar::snapped,
              treeSB, &QScrollBar::setValue);

   /* разворачивание/сворачивание */
   connect(m_signalTreeView, &SignalTreeView::itemExpandedOrCollapsed,
           m_waveView, &WaveformView::OnItemExpandedOrCollapsed);

   /* обновление значений */
   connect(m_signalModel, &SignalTreeModel::SignalsUpdated,
           this, &VcdViewerWidget::OnSignalsUpdated);

   connect(m_reader, &VcdAsyncFileReader::ReadFileReady,
           this, &VcdViewerWidget::AskForReportPreparation);
   connect(this, &VcdViewerWidget::AskForReportPreparation, this, &VcdViewerWidget::OnPrepareReport);
}

/*------------------------- helpers -----------------------------------------*/
std::vector<vcd::PinDescriptionPtr> VcdViewerWidget::SelectedPins() const
{
   std::vector<vcd::PinDescriptionPtr> list;

   QModelIndexList rows = m_pinsView->selectionModel()->selectedRows();
   list.reserve(static_cast<std::size_t>(rows.size()));

   for (int i = 0; i < rows.size(); ++i)
   {
      const QModelIndex &idx = rows.at(i);
      list.push_back(m_pinModel->At(idx.row()));
   }
   return list;
}

/*--------------------------------------------------------------------------*/
std::optional<quint64>
VcdViewerWidget::ParseTime(const QString &txt) const
{
   const QRegularExpression re(QStringLiteral(R"(^\s*([0-9]+)\s*([a-zA-Z]*)\s*$)"));
   const QRegularExpressionMatch m = re.match(txt);
   if (!m.hasMatch())
      return std::nullopt;

   bool ok = false;
   const quint64 number = m.captured(1).toULongLong(&ok);
   if (!ok)
      return std::nullopt;

   std::string unit = m.captured(2).toStdString();
   if (unit.empty())
      unit = m_timeScale.substr(m_timeScale.size() > 1 ? 1 : 0);

   const auto itUnit = s_scale.find(unit);
   const auto itCur = s_scale.find(m_timeScale.substr(m_timeScale.size() > 1 ? 1 : 0));
   if (itUnit == s_scale.end() || itCur == s_scale.end())
      return std::nullopt;

   const double ticks = static_cast<double>(number) * (itUnit->second / itCur->second);
   if (ticks < 0.5)
      return std::nullopt;

   return static_cast<quint64>(std::llround(ticks));
}

void VcdViewerWidget::OnPrepareReport(std::shared_ptr<vcd::Handle> handle)
{
   uint64_t totalSwitchings = 0;

   // Стек для обхода модулей в глубину
   std::stack<std::shared_ptr<vcd::Module>> moduleStack;
   moduleStack.push(handle->GetRootModule());

   // Вектор для хранения статистики по модулям
   std::vector<std::pair<std::string, uint64_t>> moduleStats;
   // Вектор для хранения статистики по пинам
   std::vector<std::tuple<std::string, std::string, uint64_t>> pinStats;

   // Лямбда для получения полного имени модуля (путь от корня)
   auto getFullModuleName = [](std::shared_ptr<vcd::Module> module)
   {
      std::vector<std::string> parts;
      auto current = module;
      while (current)
      {
         parts.push_back(std::string(current->GetName()));
         auto parent = current->GetParent().lock();
         current = parent;
      }
      std::reverse(parts.begin(), parts.end());
      std::string fullName;
      for (size_t i = 0; i < parts.size(); ++i)
      {
         if (i != 0)
            fullName += ".";
         fullName += parts[i];
      }
      return fullName;
   };

   // Обход дерева модулей
   while (!moduleStack.empty())
   {
      auto currentModule = moduleStack.top();
      moduleStack.pop();

      uint64_t moduleSwitches = 0;
      std::string moduleFullName = getFullModuleName(currentModule);

      // Перебираем все пины текущего модуля
      for (const auto &pin : currentModule->GetPins())
      {
         uint64_t pinSwitches = 0;
         std::string pinTypeStr;

         // Определяем тип пина и количество переключений
         if (pin->GetPinType() == vcd::PinType::parameter)
         {
            pinTypeStr = "Parameter";
            // Параметры не имеют переключений
         }
         else
         {
            // Для простых пинов
            if (auto simplePin = std::dynamic_pointer_cast<vcd::SimplePinDescription>(pin))
            {
               pinTypeStr = "Signal";
               pinSwitches = simplePin->GetTimeline().size();
            }
            // Для шин
            else if (auto busPin = std::dynamic_pointer_cast<vcd::BusPinDescription>(pin))
            {
               pinTypeStr = "Bus";
               pinSwitches = busPin->GetTimeline().size();
            }

            // Добавляем статистику по пину
            pinStats.emplace_back(moduleFullName, std::string(pin->GetName()), pinSwitches);
            moduleSwitches += pinSwitches;
         }
      }

      totalSwitchings += moduleSwitches;
      moduleStats.emplace_back(moduleFullName, moduleSwitches);

      // Добавляем подмодули в стек в обратном порядке для сохранения исходного порядка обхода
      auto subModules = currentModule->subModules();
      for (auto it = subModules.rbegin(); it != subModules.rend(); ++it)
      {
         moduleStack.push(*it);
      }
   }

   // Формируем отчет
   std::stringstream report;
   report << "Временной масштаб: " << handle->GetTimeScale() << std::endl;
   report << "Общее время моделирования: " << handle->GetMaxTs() << handle->GetTimeScale().substr(1) << std::endl;

   report << "Общее количество переключений: " << totalSwitchings << std::endl
          << std::endl;

   report << "Статистика по модулям:" << std::endl;
   for (const auto &[name, switches] : moduleStats)
   {
      report << name << ": " << switches << " переключений" << std::endl;
   }

   report << std::endl
          << "Статистика по сигналам и шинам:" << std::endl;
   for (const auto &[moduleName, pinName, switches] : pinStats)
   {
      report << moduleName << "." << pinName << ": " << switches << " переключений" << std::endl;
   }

   emit ReportReady(QString::fromStdString(report.str()));
}

/*------------------------- label update -----------------------------------*/
void VcdViewerWidget::UpdateMarkerAndCursorLabel()
{
   std::string txt;

   if (m_markerPos.has_value())
      txt += "Marker: " + std::to_string(m_markerPos.value()) +
             (m_timeScale.size() > 1 ? m_timeScale.substr(1) : "");

   if (m_cursorPos.has_value())
   {
      if (!txt.empty())
         txt += " | ";
      txt += "Cursor: " + std::to_string(m_cursorPos.value()) +
             (m_timeScale.size() > 1 ? m_timeScale.substr(1) : "");
   }
   m_lblCursorMarker->setText(QString::fromStdString(txt));
}

/*------------------------- zoom-слоты -------------------------------------*/
void VcdViewerWidget::OnZoomInClicked() { m_waveView->ZoomIn(); }
void VcdViewerWidget::OnZoomOutClicked() { m_waveView->ZoomOut(); }
void VcdViewerWidget::OnZoomResetClicked()
{
   m_prevZoomOut.store(false);
   FixZoom();
}

/*------------------------- signal-buttons ---------------------------------*/
void VcdViewerWidget::OnAppendSignals() { m_signalModel->AppendSignals(SelectedPins()); }
void VcdViewerWidget::OnReplaceSignals() { m_signalModel->ReplaceSignalsWithSelection(m_signalTreeView->selectionModel(), SelectedPins()); }
void VcdViewerWidget::OnInsertSignals()
{
   /* точно как в исходной лямбде */
   int pos = std::numeric_limits<int>::max();
   QModelIndex cur = m_signalTreeView->selectionModel()->currentIndex();
   if (cur.isValid())
   {
      pos = cur.row() + 1;
      if (cur.parent().isValid())
         pos = cur.parent().row() + 1;
   }
   m_signalModel->InsertSignals(SelectedPins(), pos);
}

/*------------------------- ModuleClicked ----------------------------------*/
void VcdViewerWidget::OnModuleClicked(std::shared_ptr<Module> module)
{
   m_pinModel->SetModule(module);
}

/*------------------------- чтение VCD -------------------------------------*/
void VcdViewerWidget::OnReadFileReady(std::shared_ptr<vcd::Handle> h)
{
   m_timeScale = h->GetTimeScale();
   if (m_timeScale.empty())
      m_timeScale = "1ns";

   m_maxTs = h->GetMaxTs();
   m_moduleModel->SetHandle(h);
   m_signalModel->SetHandle(h);
   m_pinModel->SetHandle(h);
   m_waveView->SetHandle(h);

   m_prevZoomOut.store(false);
   QTimer::singleShot(200, this, &VcdViewerWidget::FixZoom);
}

void VcdViewerWidget::OnReadFileError(const QString &msg)
{
   QMessageBox::warning(this, QStringLiteral("Read error"), msg);
}

/*------------------------- apply range ------------------------------------*/
void VcdViewerWidget::OnApplyRangeClicked()
{
   const std::optional<quint64> fromOpt = ParseTime(m_editFrom->text());
   const std::optional<quint64> toOpt = ParseTime(m_editTo->text());

   if (!fromOpt.has_value() || !toOpt.has_value())
   {
      QMessageBox::warning(this, QStringLiteral("Invalid range"),
                           QStringLiteral("Enter time as <number><unit>, e.g. 100 ns"));
      return;
   }

   quint64 from = fromOpt.value();
   quint64 to = toOpt.value();
   if (from == to)
      return;
   if (from > to)
      std::swap(from, to);

   quint64 maxTs = m_waveView->GetMaxTimestamp();
   if (from >= maxTs)
      from = maxTs - 1;
   if (to > maxTs)
      to = maxTs;
   if (from >= to)
      return;

   m_waveView->zoomToRange(from, to);
}

/*------------------------- SignalsUpdated ---------------------------------*/
void VcdViewerWidget::OnSignalsUpdated(const QVector<EmmitedSignalDescription> &descs)
{
   auto GetGsValue = [](const QString &value)
   {
      if (value == "0")
      {
         // return ...
      }
      else if (value == "1")
      {
         // return ...
      }
      else if (value == "z")
      {
         // return ...
      }
      else if (value == "x")
      {
         // return ...
      }
      else
      {
         // ...
      }
   };

   // QVector<> <- Тут вектор, который надо пробросить в globalState
   auto GetFullPinPath = [](const vcd::PinDescriptionPtr pin)
   {
      std::string fullPath{pin->GetName()};
      std::shared_ptr<vcd::Module> parent{pin->GetParent()};
      while (parent)
      {
         fullPath = std::string(parent->GetName()) + "/" + fullPath;
         parent = parent->GetParent().lock();
      }
      return QString::fromStdString(fullPath);
   };
   for (int i = 0; i < descs.size(); ++i)
   {
      const EmmitedSignalDescription &d = descs.at(i);
      const QString &name = d.name;
      const QString &value = d.value;
      std::cout << GetFullPinPath(d.pinDescription).toStdString() << std::endl;

      if (d.pinDescription->GetSignalType() == vcd::SignalType::bus) // Тут обрабатываются шины/многоразрядные регистры
      {
         if (name.contains(':'))
            continue;

         // Тут можно пихать в вектор как есть с конвертацией value с помощью функции GetGsValue()
         std::cout << name.toStdString() << ":" << value.toStdString() << std::endl;
      }
      else
      {
         // Тут можно пихать в вектор как есть с конвертацией value с помощью функции GetGsValue()
         std::cout << name.toStdString() << ":" << value.toStdString() << std::endl;
      }
   }
}

/*------------------------- FixZoom / unload / label -----------------------*/
void VcdViewerWidget::FixZoom()
{
   m_waveView->zoomToRange(0, m_maxTs);
}

void VcdViewerWidget::UnloadPreviousData()
{
   m_moduleModel->SetHandle(nullptr);
   m_signalModel->SetHandle(nullptr);
   m_pinModel->SetHandle(nullptr);
   m_waveView->SetHandle(nullptr);

   m_markerPos.reset();
   m_cursorPos.reset();
   UpdateMarkerAndCursorLabel();
}

void VcdViewerWidget::UpdateMarkerPosition(quint64 pos)
{
   m_markerPos = pos;
   UpdateMarkerAndCursorLabel();
}

void VcdViewerWidget::UpdateCursorPosition(quint64 pos)
{
   m_cursorPos = pos;
   UpdateMarkerAndCursorLabel();
}
