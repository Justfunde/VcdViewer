#ifndef __MAIN_WINDOW_H__
#define __MAIN_WINDOW_H__

#include "PinTableModel.hpp"
#include "SignalsTreeModel.hpp"
#include "TreeModulesModel.hpp"

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTableView>
#include <QTreeView>

class VcdAsyncFileReader;

class MainWindow : public QMainWindow
{
   Q_OBJECT
 signals:
   void
   AskForFileOpen(QString fileName);

 public:
   explicit MainWindow(QWidget* Parent = nullptr);

 private slots:
   void OnBrowse();

 private:
};

#endif //!__MAIN_WINDOW_H__