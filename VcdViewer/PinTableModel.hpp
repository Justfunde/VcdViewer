#ifndef __TABLE_SIGNALS_VIEW_HPP__
#define __TABLE_SIGNALS_VIEW_HPP__

#include "Include/VcdStructs.hpp"
#include <QAbstractTableModel>

/**
 * @brief Модель для отображения списка сигналов выбранного модуля в табличном виде.
 *
 * Эта модель показывает тип сигнала и его имя (с указанной при наличии битовой глубиной).
 */
class PinTableModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   /**
    * @brief Конструктор модели.
    * @param parent Родительский объект.
    */
   explicit PinTableModel(QObject *parent = nullptr)
       : QAbstractTableModel(parent)
   {
   }

public slots:

   void
   Reset()
   {
      emit beginResetModel();
      m_handle = nullptr;
      m_module = nullptr;
      emit endResetModel();
   }

   /**
    * @brief Установить текущий модуль для отображения его сигналов.
    * @param module Указатель на модуль.
    */
   void
   SetModule(std::shared_ptr<vcd::Module> module)
   {
      emit beginResetModel();
      m_module = module;
      emit endResetModel();
   }

   /**
    * @brief Установить новый VCD дескриптор, сбрасывая текущий модуль.
    * @param VcdHandle Новый дескриптор VCD.
    */
   void
   SetHandle(std::shared_ptr<vcd::Handle> VcdHandle)
   {
      emit beginResetModel();
      m_handle = VcdHandle;
      m_module = nullptr;
      emit endResetModel();
   }

   /**
    * @brief Возвращает количество строк модели (количество сигналов в модуле).
    * @param parent Родительский индекс (игнорируется, так как модель не иерархическая).
    * @return Количество сигналов или 0, если модуль не установлен.
    */
   int
   rowCount(const QModelIndex &parent = QModelIndex()) const override
   {
      if (parent.isValid() || !m_module)
      {
         return 0;
      }
      return static_cast<int>(m_module->GetPins().size());
   }

   /**
    * @brief Возвращает количество столбцов модели.
    * @param parent Родительский индекс (игнорируется).
    * @return Количество столбцов (2: для типа и для имени сигнала).
    */
   int
   columnCount(const QModelIndex &parent = QModelIndex()) const override
   {
      Q_UNUSED(parent);
      return 2;
   }

   /**
    * @brief Возвращает данные для отображения в ячейке.
    * @param index Индекс модели.
    * @param role Роль данных.
    * @return Данные для отображения (тип сигнала в колонке 0, имя сигнала в колонке 1).
    */
   QVariant
   data(const QModelIndex &index, int role = Qt::DisplayRole) const override
   {
      if (!index.isValid() || !m_module || role != Qt::DisplayRole)
      {
         return QVariant();
      }

      const auto pin = m_module->GetPins().at(index.row());

      if (index.column() == 0)
      {
         return QString::fromStdString(pinTypeToString(pin->GetPinType()));
      }
      else if (index.column() == 1)
      {
         std::string resultName = std::string(pin->GetName());
         if (pin->GetSignalType() == vcd::SignalType::bus)
         {
            const auto &bitDepth = std::static_pointer_cast<vcd::BusPinDescription>(pin)->GetBitDepth();
            resultName += "[" + std::to_string(bitDepth.first) + ":" + std::to_string(bitDepth.second) + "]";
         }
         return QString::fromStdString(resultName);
      }

      return QVariant();
   }

   /**
    * @brief Возвращает данные заголовка столбцов.
    * @param section Индекс столбца.
    * @param orientation Ориентация (горизонтальная для заголовков столбцов).
    * @param role Роль данных.
    * @return Названия столбцов: "Type" и "Signals".
    */
   QVariant
   headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
   {
      if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
      {
         if (section == 0)
         {
            return QString("Type");
         }
         else if (section == 1)
         {
            return QString("Signals");
         }
      }
      return QVariant();
   }

   vcd::PinDescriptionPtr
   At(std::size_t idx)
   {
      if (idx >= m_module->GetPins().size())
      {
         return nullptr;
      }
      return m_module->GetPins().at(idx);
   }

public slots:
   /**
    * @brief Слот, вызываемый при клике по элементу таблицы.
    * @param index Индекс модели, по которому кликнуsли.
    *
    * Извлекает соответствующий сигнал и эмитирует сигнал PinClicked.
    */
   void
   OnItemClicked(const QModelIndex &index)
   {
      if (index.isValid())
      {
         emit PinClicked(m_module->GetPins().at(index.row()));
      }
   }

signals:
   /**
    * @brief Сигнал, испускаемый при клике на конкретный сигнал.
    * @param module Указатель на выбранный сигнал (PinDescriptionPtr).
    */
   void PinClicked(vcd::PinDescriptionPtr module);

private:
   /**
    * @brief Преобразование типа сигнала vcd::PinType в строку.
    * @param type Тип пина.
    * @return Строковое представление типа сигнала.
    */
   std::string
   pinTypeToString(vcd::PinType type) const
   {
      switch (type)
      {
      case vcd::PinType::wire:
         return "wire";
      case vcd::PinType::integer:
         return "integer";
      case vcd::PinType::reg:
         return "reg";
      case vcd::PinType::parameter:
         return "parm";
      default:
         return "UNKNOWN";
      }
   }

   std::shared_ptr<vcd::Module> m_module; ///< Текущий отображаемый модуль.
   std::shared_ptr<vcd::Handle> m_handle; ///< Текущий дескриптор VCD.
};

Q_DECLARE_METATYPE(vcd::PinDescriptionPtr)

#endif //!__TABLE_SIGNALS_VIEW_HPP__
