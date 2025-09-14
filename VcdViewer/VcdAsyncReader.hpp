#pragma once
/*****************************************************************************
 *  VcdAsyncFileReader
 *  ------------------
 *  Асинхронный загрузчик VCD-файла, выполняющий тяжёлый разбор в пуле потоков
 *  и отправляющий готовый `std::shared_ptr<vcd::Handle>` обратно в
 *  GUI-поток через сигнал Qt.
 *****************************************************************************/

#include <QObject>
#include <QString>
#include <memory>

#include "Include/VcdStructs.hpp" // объявление vcd::Handle

/**
 * @brief Асинхронно читает VCD-файл и уведомляет о результате.
 *
 * Пример использования:
 * @code
 * auto reader = new VcdAsyncFileReader(this);
 * connect(reader, &VcdAsyncFileReader::ReadFileReady,
 *         this,   &MyWidget::onHandleReady);
 * QMetaObject::invokeMethod(reader, "ReadFile",
 *         Qt::QueuedConnection, Q_ARG(QString, "/path/to/file.vcd"));
 * @endcode
 */
class VcdAsyncFileReader final : public QObject
{
   Q_OBJECT
public:
   explicit VcdAsyncFileReader(QObject *parent = nullptr);

public slots:
   /**
    * @brief Запускает парсинг указанного VCD-файла.
    * @param vcdFilePath Путь к *.vcd.
    *
    * Генерирует:
    *  * ReadFileReady(shared_ptr<Handle>) — если успех;
    *  * ReadFileError(QString)            — если ошибка.
    */
   void ReadFile(const QString &vcdFilePath);

signals:
   /// Файл успешно прочитан, `handle` готов к работе.
   void ReadFileReady(std::shared_ptr<vcd::Handle> handle);

   /// Произошла ошибка; текст содержит описание.
   void ReadFileError(QString description);
};

// Регистрируем тип для queued-сигналов.
Q_DECLARE_METATYPE(std::shared_ptr<vcd::Handle>)
