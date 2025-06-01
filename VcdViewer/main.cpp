#include <QApplication>
#include <QDebug>

#include "MainWindow.hpp"

int
main(int argc, char* argv[])
{
   QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
   QApplication app(argc, argv);

   MainWindow w;
   qDebug() << "This is a debug message";

   w.show();
   return app.exec();
}