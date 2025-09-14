#include "VcdAsyncReader.hpp"

#include <QtConcurrent>
#include <filesystem>
#include <qmetatype.h>

VcdAsyncFileReader::VcdAsyncFileReader(QObject *parent)
    : QObject(parent)
{
}

/*-------------------------------------------------------------------------*/
void VcdAsyncFileReader::ReadFile(const QString &vcdFilePath)
{
    const std::filesystem::path filePath = vcdFilePath.toStdString();

    if (!std::filesystem::exists(filePath))
    {
        emit ReadFileError(tr("Файл не существует"));
        return;
    }

    /* 2. Отправляем парсинг в пул потоков                       */
    /*    QtConcurrent гарантирует queued-delivery сигнала назад */
    QtConcurrent::run([this, filePath]()
                      {
        try
        {
            auto handle = std::make_shared<vcd::Handle>();

            /* Последовательность инициализации — как и раньше */
            handle->Init(filePath);
            handle->LoadHdr();
            handle->LoadSignalsParallel();

            emit ReadFileReady(handle);   // queued-connection
        }
        catch (const std::exception &ex)
        {
            emit ReadFileError(QString::fromStdString(ex.what()));
        } });
}