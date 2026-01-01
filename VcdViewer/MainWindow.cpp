#include "WaveformView.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include "MainWindow.hpp"

#include "VcdViewerWidget.hpp"

MainWindow::MainWindow(QWidget *Parent)
    : QMainWindow(Parent)
{
   setWindowTitle("VCD Module and Signal Viewer");
   resize(1400, 1300);
   qRegisterMetaType<std::shared_ptr<vcd::Handle>>("std::shared_ptr<vcd::Handle>");
   VcdViewerWidget *viewerWidget = new VcdViewerWidget;

   // Устанавливаем центральный виджет
   QWidget *centralWidget = new QWidget(this);
   setCentralWidget(centralWidget);

   QVBoxLayout *mainLayout = new QVBoxLayout;
   centralWidget->setLayout(mainLayout);

   // Создаем кнопку "Выбрать файл"
   QPushButton *buttonBrowse = new QPushButton(tr("Выбрать файл"));
   buttonBrowse->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

   QPushButton *buttonReset = new QPushButton(tr("Сбросить Waveform"));
   buttonReset->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

   QHBoxLayout *buttonsLayout = new QHBoxLayout;
   buttonsLayout->addSpacerItem(new QSpacerItem(5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum));
   buttonsLayout->addWidget(buttonBrowse);
   buttonsLayout->addWidget(buttonReset);

   QWidget *buttonWidget = new QWidget;
   buttonWidget->setLayout(buttonsLayout);
   buttonWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

   mainLayout->addWidget(buttonWidget, 0);
   mainLayout->addWidget(viewerWidget, 1);

   connect(buttonReset, &QPushButton::clicked, viewerWidget, &VcdViewerWidget::UnloadPreviousData);
   connect(this, &MainWindow::AskForFileOpen, viewerWidget, &VcdViewerWidget::AskForReadFile);
   connect(buttonBrowse, &QPushButton::clicked, this, [this]()
           {
      // Открываем диалог выбора файла
      QString filePath = QFileDialog::getOpenFileName(this, tr("Выберите файл"), QString(""), tr("Все файлы (*.*)"));
      if (!filePath.isEmpty())
      {
         // Эмитируем сигнал с выбранным файлом
         emit AskForFileOpen(filePath);
      } });
   emit AskForFileOpen("/home/justfunde/Work/Miet/VcdViewer/VcdTests/bugs/c432.vcd");
}

void MainWindow::OnBrowse()
{
   QString fileName = QFileDialog::getOpenFileName(this, tr("Vcd files (*.vcd)"));
   if (fileName.isEmpty())
   {
      return;
   }
}
