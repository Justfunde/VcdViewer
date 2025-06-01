#ifndef __VCD_ASYNC_READER_HPP__
#define __VCD_ASYNC_READER_HPP__

#include <QObject>
#include <QString>
#include <QtConcurrent>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>

#include "Include/VcdStructs.hpp"

class VcdAsyncFileReader : public QObject
{
   Q_OBJECT

 public:
   explicit VcdAsyncFileReader(QObject* Parent = nullptr)
      : QObject(Parent)
   {
   }

 signals:
   void ReadFileReady(std::shared_ptr<newVcd::Handle>);
   void ReadFileError(QString);

 public slots:
   void
   ReadFile(QString VcdFilePath)
   {
      std::filesystem::path filePath = VcdFilePath.toStdString();

      if (!std::filesystem::exists(filePath))
      {
         emit ReadFileError("Файл не существует");
         return;
      }

      try
      {
         auto handle = std::make_shared<newVcd::Handle>();
         handle->Init(filePath);
         handle->LoadHdr();
         emit ReadFileReady(handle);
      }
      catch (const std::exception& ex)
      {
         std::cerr << ex.what();
         emit ReadFileError(QString::fromStdString(ex.what()));
      }
   }
};

Q_DECLARE_METATYPE(std::shared_ptr<newVcd::Handle>)

#endif // __VCD_ASYNC_READER_HPP__
