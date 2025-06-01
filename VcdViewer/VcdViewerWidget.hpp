#ifndef __VCD_VIEWER_WIDGET_HPP__
#define __VCD_VIEWER_WIDGET_HPP__

#include "PinTableModel.hpp"
#include "SignalsTreeModel.hpp"
#include "TreeModulesModel.hpp"
#include "VcdAsyncReader.hpp"
#include <QTimer>

#include "Waveform.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

#include "VcdAsyncReader.hpp"

class VcdAsyncFileReader;

using namespace newVcd;

class VcdViewerWidget : public QWidget
{
   Q_OBJECT

 public:
 signals:
   void
      AskForReadFile(QString);

 public:
   VcdViewerWidget(QWidget* Parent = nullptr)
      : QWidget(Parent)
      , m_vcdReader(new VcdAsyncFileReader(this))
      , m_modulesView(new QTreeView(this))
      , m_signalTreeView(new SignalTreeView(this))
      , m_pinsView(new QTableView(this))
      , m_pinTableModel(new PinTableModel(this))
   {
      setWindowTitle("VCD Module and Signal Viewer");
      resize(1400, 1300);

      qRegisterMetaType<PinDescriptionPtr>("PinDescriptionPtr");

      // Устанавливаем центральный виджет
      QVBoxLayout* mainLayout = new QVBoxLayout;
      setLayout(mainLayout);

      // Создаем кнопку "Выбрать файл"
      QPushButton* buttonZoomIn = new QPushButton;
      QPushButton* buttonZoomOut = new QPushButton;
      QPushButton* buttonResetZoom = new QPushButton;

      QIcon zoomInIcon = QIcon(":icons/zoomIn.ico");
      buttonZoomIn->setIcon(zoomInIcon);

      QIcon zoomOutIcon = QIcon(":icons/zoomOut.ico");
      buttonZoomOut->setIcon(zoomOutIcon);

      QIcon resetZoomIcon = QIcon(":icons/zoomReset.ico");
      buttonResetZoom->setIcon(resetZoomIcon);

      QHBoxLayout* buttonLayout = new QHBoxLayout;
      buttonLayout->addWidget(buttonZoomIn);
      buttonLayout->addWidget(buttonZoomOut);
      buttonLayout->addWidget(buttonResetZoom);
      buttonLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

      m_modulesView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

      m_signalTreeView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

      m_wfView = new WaveformView;

      QVBoxLayout* wfLayout = new QVBoxLayout;
      wfLayout->addWidget(m_wfView->GetScaleView());
      wfLayout->addWidget(m_wfView);
      wfLayout->setContentsMargins(0, 0, 0, 0);
      wfLayout->setSpacing(0);
      m_wfView->setFrameStyle(QFrame::NoFrame);
      m_wfView->GetScaleView()->setFrameStyle(QFrame::NoFrame);

      QHBoxLayout* signalViewLayout = new QHBoxLayout();
      signalViewLayout->addWidget(m_signalTreeView);
      signalViewLayout->addLayout(wfLayout);

      QPushButton* insertSignalBtn = new QPushButton("Insert");
      insertSignalBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

      QPushButton* replaceSignalBtn = new QPushButton("Replace");
      replaceSignalBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

      QPushButton* appendSignalBtn = new QPushButton("Append");
      appendSignalBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

      QHBoxLayout* opBtnLayout = new QHBoxLayout;
      opBtnLayout->addWidget(appendSignalBtn);
      opBtnLayout->addWidget(insertSignalBtn);
      opBtnLayout->addWidget(replaceSignalBtn);

      QVBoxLayout* inputTableLayout = new QVBoxLayout();
      inputTableLayout->addWidget(m_modulesView);
      inputTableLayout->addWidget(m_pinsView);
      inputTableLayout->addLayout(opBtnLayout);

      QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
      splitter->setChildrenCollapsible(false); // Отключить возможность полного сворачивания

      // Создаем виджеты для размещения макетов
      QWidget* inputTableWidget = new QWidget(this);
      inputTableWidget->setLayout(inputTableLayout);

      QWidget* signalViewWidget = new QWidget(this);
      signalViewWidget->setLayout(signalViewLayout);

      // Добавляем виджеты в сплиттер
      splitter->addWidget(inputTableWidget);
      splitter->addWidget(signalViewWidget);

      // Устанавливаем минимальные размеры для предотвращения полного сворачивания
      inputTableWidget->setMinimumWidth(200); // Минимальная ширина для inputTableLayout
      signalViewWidget->setMinimumWidth(400); // Минимальная ширина для signalViewLayout

      // Устанавливаем веса для достижения соотношения 2:1
      splitter->setStretchFactor(0, 1); // Вес для inputTableWidget (1)
      splitter->setStretchFactor(1, 2); // Вес для signalViewWidget (2)

      mainLayout->addLayout(buttonLayout);
      mainLayout->addWidget(splitter);

      // Дерево модулей
      m_modulesView->setHeaderHidden(true);

      // Таблица сигналов
      m_pinsView->setModel(m_pinTableModel);
      m_pinsView->setSelectionBehavior(QAbstractItemView::SelectRows);
      m_pinsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      for (int32_t c = 0; c < m_pinsView->horizontalHeader()->count(); ++c)
      {
         m_pinsView->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);
      }

      // Настройка дерева модулей и модели

      m_modulesTreeModel = new ModuleTreeModel;

      m_modulesView->setModel(m_modulesTreeModel);

      m_signalTreeModel = new SignalTreeModel;

      m_signalTreeView->setModel(m_signalTreeModel);

      // Связываем полосы прокрутки
      QScrollBar* treeScrollBar = m_signalTreeView->GetVerticalScrollBar();
      QScrollBar* waveformScrollBar = m_wfView->GetVerticalScrollBar();

      // Синхронизация полос прокрутки
      QObject::connect(treeScrollBar, &QScrollBar::valueChanged, waveformScrollBar, &QScrollBar::setValue);
      QObject::connect((SnapScrollBar*)waveformScrollBar, &SnapScrollBar::snapped, treeScrollBar, &QScrollBar::setValue);

      auto getSelectedPins = [=]()
      {
         std::vector<newVcd::PinDescriptionPtr> ret;

         auto selectedList = m_pinsView->selectionModel()->selectedRows();
         ret.reserve(selectedList.size());

         for (const auto& it : selectedList)
         {
            ret.emplace_back(m_pinTableModel->At(it.row()));
         }
         return ret;
      };

      connect(appendSignalBtn, &QPushButton::clicked, this, [=]()
      {
         m_signalTreeModel->AppendSignals(getSelectedPins());
      });
      connect(replaceSignalBtn, &QPushButton::clicked, this, [=]()
      {
         m_signalTreeModel->ReplaceSignalsWithSelection(m_signalTreeView->selectionModel(), getSelectedPins());
      });
      connect(insertSignalBtn, &QPushButton::clicked, this, [=]()
      {
         QModelIndex current = m_signalTreeView->selectionModel()->currentIndex();
         int pos = std::numeric_limits<int>::max();

         if (current.isValid())
         {
            pos = current.row() + 1;

            if (current.parent().isValid())
            {
               pos = current.parent().row() + 1;
            }
         }
         m_signalTreeModel->InsertSignals(getSelectedPins(), pos);
      });
      // Соединяем сигналы и слоты
      connect(this, &VcdViewerWidget::AskForReadFile, m_vcdReader, &VcdAsyncFileReader::ReadFile);
      connect(m_vcdReader, &VcdAsyncFileReader::ReadFileError, this, [this](QString str)
      { qDebug() << "Error in reading file: " << str; });
      connect(m_vcdReader, &VcdAsyncFileReader::ReadFileReady, this, [=](auto handle)
      {
         qDebug() << "File was successfully read: ";
         m_modulesTreeModel->SetHandle(handle);
         m_signalTreeModel->SetHandle(handle);
         m_pinTableModel->SetHandle(handle);
         m_wfView->SetHandle(handle);
         m_prevZoomOut.store(false);
         QTimer::singleShot(200, [this]()
         { this->FixZoom(); });
      });

      connect(m_modulesView, &QTreeView::clicked, m_modulesTreeModel, &ModuleTreeModel::OnItemClicked);
      connect(m_modulesTreeModel, &ModuleTreeModel::ModuleClicked, this, [this](std::shared_ptr<Module> module)
      {
         m_pinTableModel->SetModule(module);
      });

      connect(m_pinsView, &QTreeView::doubleClicked, m_pinTableModel, &PinTableModel::OnItemClicked);
      connect(m_pinTableModel, &PinTableModel::PinClicked, m_signalTreeModel, &SignalTreeModel::AppendSignal);
      connect(m_signalTreeModel, &SignalTreeModel::SignalListChanged, m_wfView, &WaveformView::UpdateSignals);
      connect(m_wfView, &WaveformView::SelectedTimestampChange, m_signalTreeModel, &SignalTreeModel::SetSelectedTimestamp);
      connect(buttonZoomIn, &QPushButton::clicked, m_wfView, &WaveformView::ZoomIn);
      connect(buttonZoomOut, &QPushButton::clicked, m_wfView, &WaveformView::ZoomOut);
      connect(buttonResetZoom, &QPushButton::clicked, this, [this]()
      { m_prevZoomOut.store(false); FixZoom(); });
      connect(m_signalTreeView, &SignalTreeView::itemExpandedOrCollapsed, m_wfView, &WaveformView::OnItemExpandedOrCollapsed);

      connect(
         m_signalTreeModel,
         &SignalTreeModel::SignalsUpdated,
         this,
         [this](QVector<EmmitedSignalDescription> Signals)
      {
         auto GetGsValue = [](const QString& value)
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
         for (const auto& it : Signals)
         {
            const QString& name = it.name;
            const QString& value = it.value;
            // if (it.pinDescription->bitDepth.has_value()) // Тут обрабатываются шины/многоразрядные регистры
            //{
            //    // Тут я не знаю, в каком виде передавать шины. Допустим сигналы верхнего уровня модели я пропускаю (прим. counter[4:0]) и обрабатываю только разворот дерева (прим. counter[4],counter[3], counter[2], ...)
            //    if (name.contains(":"))
            //    {
            //       continue;
            //    }
            //    // Тут можно пихать в вектор как есть с конвертацией value с помощью функции GetGsValue()
            //    std::cout << name.toStdString() << ":" << value.toStdString() << std::endl;
            // }
            // else
            //{
            //    // Тут можно пихать в вектор как есть с конвертацией value с помощью функции GetGsValue()
            //    std::cout << name.toStdString() << ":" << value.toStdString() << std::endl;
            // }
         }
      });

      emit AskForReadFile("/home/justfunde/Projects/vcd/VcdTests/c432.gates.flat.synth - XXL.vcd");
      //  Устанавливаем заголовок окна и размеры
   }

 public slots:
   void
   FixZoom()
   {
      if (m_wfView->HasScrollBar())
      {
         if (m_wfView->ZoomOut())
         {
            m_prevZoomOut.store(true);
            QTimer::singleShot(10, [this]()
            { FixZoom(); });
         }
      }
      else if (!m_prevZoomOut.load())
      {
         m_wfView->ZoomIn();
         m_prevZoomOut.store(false);
         QTimer::singleShot(10, [this]()
         { FixZoom(); });
      }
   }

   void
   UnloadPreviousData()
   {
      m_modulesTreeModel->SetHandle(nullptr);
      m_signalTreeModel->SetHandle(nullptr);
      m_pinTableModel->SetHandle(nullptr);
      m_wfView->SetHandle(nullptr);
   }

 private:
   VcdAsyncFileReader* m_vcdReader;
   QTreeView* m_modulesView;
   QTableView* m_pinsView;
   SignalTreeView* m_signalTreeView;
   ModuleTreeModel* m_modulesTreeModel;

   PinTableModel* m_pinTableModel;
   SignalTreeModel* m_signalTreeModel;
   VcdAsyncFileReader* m_reader;
   WaveformView* m_wfView;

   std::atomic_bool m_prevZoomOut = false;
};

#endif //!__VCD_VIEWER_WIDGET_HPP__