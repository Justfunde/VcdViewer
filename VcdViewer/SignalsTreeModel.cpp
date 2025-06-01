#include "SignalsTreeModel.hpp"
#include <QDebug>
#include <QModelIndex>
#include <QVariant>

/**************************************
 *          SignalTreeModel
 **************************************/

SignalTreeModel::SignalTreeModel(QObject* parent)
   : QAbstractItemModel(parent)
{
}

void
SignalTreeModel::SetSignals(const std::vector<newVcd::PinDescriptionPtr>& signalVector)
{
   beginResetModel();
   m_signals = signalVector;
   endResetModel();
}

newVcd::PinDescriptionPtr
SignalTreeModel::GetPinDescriptionFromIndex(const QModelIndex& index) const
{
   if (!index.isValid())
      return nullptr;

   int parentRow = -1;
   if (index.internalPointer() == nullptr)
   {
      // Верхний уровень
      parentRow = index.row();
   }
   else
   {
      // Дочерний элемент
      parentRow = static_cast<int>(reinterpret_cast<qintptr>(index.internalPointer())) - 1;
   }

   if (parentRow >= 0 && parentRow < static_cast<int>(m_signals.size()))
   {
      return m_signals.at(parentRow);
   }

   return nullptr;
}

void
SignalTreeModel::UpdateAllData(const QModelIndex& parent)
{
   int rows = rowCount(parent);
   int columns = columnCount(parent);

   if (rows > 0 && columns > 0)
   {
      QModelIndex topLeft = index(0, 0, parent);
      QModelIndex bottomRight = index(rows - 1, columns - 1, parent);
      emit dataChanged(topLeft, bottomRight);

      // Рекурсивно обновляем дочерние элементы
      for (int row = 0; row < rows; ++row)
      {
         QModelIndex childParent = index(row, 0, parent);
         if (hasChildren(childParent))
         {
            UpdateAllData(childParent);
         }
      }
   }
}

void
SignalTreeModel::emitSignalsAndValues()
{
   QVector<EmmitedSignalDescription> result;
   if (!m_timestamp.has_value())
   {
      emit SignalsUpdated(result);
      return;
   }

   // Лямбда-функция для рекурсивного обхода
   std::function<void(const QModelIndex&)> traverse = [&](const QModelIndex& parentIndex)
   {
      int rows = this->rowCount(parentIndex);
      int cols = this->columnCount(parentIndex);

      for (int r = 0; r < rows; ++r)
      {
         for (int c = 0; c < cols; ++c)
         {
            QModelIndex idx = this->index(r, c, parentIndex);
            if (!idx.isValid())
               continue;

            QString text = this->data(idx, Qt::DisplayRole).toString();
            if (!text.isEmpty())
            {
               // Разбиваем по '='
               QString before = text;
               QString after;
               int equalPos = text.indexOf('=');
               if (equalPos != -1)
               {
                  before = text.left(equalPos).trimmed();
                  after = text.mid(equalPos + 1).trimmed();
               }

               result.append({GetPinDescriptionFromIndex(idx), before, after});
            }
            qWarning() << text;

            // Рекурсивно обходим детей
            if (this->hasChildren(idx))
            {
               traverse(idx);
            }
         }
      }
   };

   // Начинаем с корневого индекса
   traverse(QModelIndex());
   emit SignalsUpdated(result);
}

void
SignalTreeModel::SetSelectedTimestamp(uint64_t ts)
{
   m_timestamp = ts;
   UpdateAllData();
   emitSignalsAndValues();
}

void
SignalTreeModel::AppendSignal(newVcd::PinDescriptionPtr sig)
{
   if (std::find_if(m_signals.begin(), m_signals.end(), [&sig](auto it)
   { return sig->alias() == it->alias(); }) != m_signals.end())
   {
      return;
   }
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   m_signals.push_back(sig);
   endInsertRows();
   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::AppendSignals(const std::vector<newVcd::PinDescriptionPtr>& sigs)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   for (newVcd::PinDescriptionPtr sig : sigs)
   {
      if (std::find_if(m_signals.begin(), m_signals.end(), [sig](auto it)
      { return sig->alias() == it->alias(); }) != m_signals.end())
      {
         continue;
      }
      m_signals.push_back(sig);
   }

   endInsertRows();
   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::ReplaceSignalsWithSelection(
   QItemSelectionModel* selectionModel,
   const std::vector<newVcd::PinDescriptionPtr>& newSignals)
{
   if (!selectionModel)
   {
      return;
   }

   // Получаем список выбранных индексов
   QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
   if (selectedIndexes.isEmpty())
   {
      return;
   }

   // Убираем дубликаты индексов, если вдруг что-то выбрано столбцами / битами
   // (или пользователь переключил представление). Также сортируем по row.
   std::sort(selectedIndexes.begin(), selectedIndexes.end(),
             [](const QModelIndex& a, const QModelIndex& b)
   {
      return a.row() < b.row();
   });
   selectedIndexes.erase(std::unique(selectedIndexes.begin(), selectedIndexes.end()),
                         selectedIndexes.end());

   // Вычисляем минимальный и максимальный индекс (по строке),
   // чтобы одним диапазоном удалить все выбранные сигналы.
   const int beginRow = selectedIndexes.first().row();
   const int endRow = selectedIndexes.last().row();

   // Проверим валидность
   if (beginRow < 0 || endRow >= static_cast<int>(m_signals.size()) || beginRow > endRow)
   {
      return;
   }

   // 1) Удаляем старые сигналы (диапазон [beginRow, endRow]).
   beginRemoveRows(QModelIndex(), beginRow, endRow);
   m_signals.erase(
      m_signals.begin() + beginRow,
      m_signals.begin() + endRow + 1);
   endRemoveRows();

   // 2) Фильтруем новые сигналы, чтобы не вставлять те, что уже есть
   //    (по полю alias). Если вы хотите всегда вставлять всё подряд,
   //    уберите этот шаг.
   std::vector<newVcd::PinDescriptionPtr> uniqueNew;
   uniqueNew.reserve(newSignals.size());
   for (auto& sig : newSignals)
   {
      auto it = std::find_if(m_signals.begin(), m_signals.end(),
                             [sig](const newVcd::PinDescriptionPtr& existing)
      {
         return existing->alias() == sig->alias();
      });
      if (it == m_signals.end())
      {
         uniqueNew.push_back(sig);
      }
   }

   // Если после фильтра нечего вставлять - на этом всё
   if (uniqueNew.empty())
   {
      // Но при этом сигналы уже были удалены, так что модель обновлена.
      return;
   }

   // 3) Вставляем новые сигналы в позицию beginRow
   beginInsertRows(QModelIndex(), beginRow, beginRow + static_cast<int>(uniqueNew.size()) - 1);
   m_signals.insert(
      m_signals.begin() + beginRow,
      uniqueNew.begin(),
      uniqueNew.end());
   endInsertRows();

   // По желанию, если у вас есть сигнал об изменении списка:
   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::InsertSignals(
   const std::vector<newVcd::PinDescriptionPtr>& sigs,
   std::size_t pos)
{
   if (sigs.empty())
   {
      return;
   }

   // 1) Фильтруем входные сигналы, убирая уже существующие (по alias).
   std::vector<newVcd::PinDescriptionPtr> uniqueSigs;
   uniqueSigs.reserve(sigs.size());
   for (const auto& sig : sigs)
   {
      // Проверяем, нет ли уже такого alias
      auto it = std::find_if(m_signals.begin(), m_signals.end(),
                             [&sig](const newVcd::PinDescriptionPtr& existing)
      {
         return existing->alias() == sig->alias();
      });
      if (it == m_signals.end())
      {
         uniqueSigs.push_back(sig);
      }
   }

   if (uniqueSigs.empty())
   {
      // Все сигналы дублируют имеющиеся — ничего вставлять
      return;
   }

   // 2) Скорректируем pos, чтобы не выйти за пределы массива
   if (pos > m_signals.size())
   {
      pos = m_signals.size();
   }

   // 3) Уведомляем о вставке строк
   beginInsertRows(QModelIndex(),
                   static_cast<int>(pos),
                   static_cast<int>(pos + uniqueSigs.size() - 1));

   // 4) Собственно вставляем
   m_signals.insert(
      m_signals.begin() + static_cast<std::ptrdiff_t>(pos),
      uniqueSigs.begin(),
      uniqueSigs.end());

   // 5) Завершаем уведомление
   endInsertRows();

   // 6) По желанию можно уведомить слушателей о том, что список сигналов поменялся
   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::RemoveSignal(const QModelIndex& index)
{
   if (!index.isValid() || index.row() >= static_cast<int>(m_signals.size()))
      return;

   beginRemoveRows(QModelIndex(), index.row(), index.row());
   m_signals.erase(m_signals.begin() + index.row());
   endRemoveRows();

   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::RemoveSignals(const QModelIndexList& idxs)
{
   int beginRow = 0;
   int endRow = 0;
   int offset = 0;
   bool changed = false;
   for (const auto& idx : idxs)
   {
      if (!idx.isValid() || idx.parent().isValid())
      {
         continue;
      }

      m_signals.erase(m_signals.begin() + idx.row() - (offset++));
      beginRow = std::min(idx.row(), beginRow);
      endRow = std::max(idx.row(), endRow);
      changed = true;
   }
   if (!changed)
   {
      return;
   }
   beginRemoveRows(QModelIndex(), beginRow, endRow);
   endRemoveRows();
   emit SignalListChanged(m_signals);
   emitSignalsAndValues();
}

void
SignalTreeModel::SetHandle(std::shared_ptr<newVcd::Handle> VcdHandle)
{
   beginResetModel();
   m_handle = VcdHandle;
   m_signals.clear();
   m_timestamp = std::nullopt;
   endResetModel();
   emitSignalsAndValues();
}

int
SignalTreeModel::rowCount(const QModelIndex& parent) const
{
   if (!parent.isValid())
   {
      // Верхний уровень
      return static_cast<int>(m_signals.size());
   }
   else
   {
      if (parent.internalPointer() == nullptr)
      {
         // Родитель — верхнеуровневый элемент
         int parentRow = parent.row();
         if (parentRow >= 0 && parentRow < static_cast<int>(m_signals.size()))
         {
            const auto& pin = m_signals.at(parentRow);
            if (pin->sigType() == newVcd::SigType::complex)
            {
               const auto bitDepth = std::static_pointer_cast<newVcd::MultiplePinDescription>(pin)->bitDepth();
               return static_cast<int>(bitDepth.first - bitDepth.second + 1);
            }
         }
      }
      else
      {
         // Дочерний элемент, других уровней нет
         return 0;
      }
   }
   return 0;
}

int
SignalTreeModel::columnCount(const QModelIndex& parent) const
{
   Q_UNUSED(parent);
   return 1;
}

QVariant
SignalTreeModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid() || role != Qt::DisplayRole)
   {
      return QVariant();
   }

   newVcd::PinDescriptionPtr pin = GetPinDescriptionFromIndex(index);
   if (!pin)
      return QVariant();

   QString displayName = QString::fromStdString(std::string(pin->name()));

   if (index.internalPointer() == nullptr)
   {
      // Верхний уровень
      if (pin->sigType() == newVcd::SigType::complex)
      {
         const auto& bitDepth = std::static_pointer_cast<newVcd::MultiplePinDescription>(pin)->bitDepth();
         displayName += "[" + QString::number(bitDepth.first) + ":" + QString::number(bitDepth.second) + "]";
      }
      if (m_timestamp.has_value())
      {
         QString value;
         if (pin->pinType() == newVcd::PinType::parameter)
         {
            value = QString::number(std::stoi(pin->initState(), 0, 2), 16).toUpper();
         }
         else if (pin->sigType() == newVcd::SigType::complex)
         {
            auto multiplePin = std::static_pointer_cast<newVcd::MultiplePinDescription>(pin);
            std::string tmpVal = std::string(multiplePin->getValueBus(m_timestamp.value()));
            if (std::all_of(tmpVal.begin(), tmpVal.end(), isdigit))
            {
               value = QString::number(std::stoi(tmpVal, 0, 2), 16).toUpper();
            }
            else
            {
               value = QString::fromStdString(tmpVal);
            }
         }
         else
         {
            auto simplePin = std::static_pointer_cast<newVcd::SimplePinDescription>(pin);
            value = QString(simplePin->getValueChar(m_timestamp.value()));
         }
         displayName += " = " + value;
      }
   }
   else
   {
      //// Уровень битов
      // int bitIndex = pin->bitDepth->first - index.row();
      // displayName += "[" + QString::number(bitIndex) + "]";
      // if (m_timestamp.has_value())
      //{
      //    char value = m_handle->GetPinValueAtTs(m_timestamp.value(), pin, bitIndex);
      //    displayName += " = " + QString(value);
      // }
   }
   return displayName;
}

QVariant
SignalTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
   {
      return QString("Signals");
   }
   return QVariant();
}

QModelIndex
SignalTreeModel::index(int row, int column, const QModelIndex& parent) const
{
   if (!hasIndex(row, column, parent))
      return QModelIndex();

   if (!parent.isValid())
   {
      // Верхний уровень
      return createIndex(row, column, nullptr);
   }
   else
   {
      if (parent.internalPointer() == nullptr)
      {
         // Уровень битов
         return createIndex(row, column, reinterpret_cast<void*>(static_cast<qintptr>(parent.row()) + 1));
      }
      else
      {
         // Нет дальнейших уровней
         return QModelIndex();
      }
   }
}

QModelIndex
SignalTreeModel::parent(const QModelIndex& index) const
{
   if (!index.isValid())
      return QModelIndex();

   if (index.internalPointer() == nullptr)
   {
      // Верхнеуровневый элемент, без родителя
      return QModelIndex();
   }
   else
   {
      // Дочерний элемент, родитель — верхнеуровневый
      int parentRow = static_cast<int>(reinterpret_cast<qintptr>(index.internalPointer())) - 1;
      return createIndex(parentRow, 0, nullptr);
   }
}

/**************************************
 *         FixedHeightDelegate
 **************************************/

FixedHeightDelegate::FixedHeightDelegate(int fixedHeight, QObject* parent)
   : QStyledItemDelegate(parent)
   , m_fixedHeight(fixedHeight)
{
}

QSize
FixedHeightDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   QSize size = QStyledItemDelegate::sizeHint(option, index);
   size.setHeight(m_fixedHeight);
   return size;
}

/**************************************
 *          SignalTreeView
 **************************************/

SignalTreeView::SignalTreeView(QWidget* parent)
   : QTreeView(parent)
{

   header()->setFixedHeight(30);
   const int fixedRowHeight = 30;
   setItemDelegate(new FixedHeightDelegate(fixedRowHeight, this));

   setUniformRowHeights(true);
   setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
   setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
   setSelectionMode(QAbstractItemView::SelectionMode::ContiguousSelection);

   connect(this, &QTreeView::expanded, this, &SignalTreeView::onItemExpanded);
   connect(this, &QTreeView::collapsed, this, &SignalTreeView::onItemCollapsed);
   connect(this, &QTreeView::customContextMenuRequested, this, &SignalTreeView::OnCustomMenuRequested);
}

QScrollBar*
SignalTreeView::GetVerticalScrollBar() const
{
   return verticalScrollBar();
}

void
SignalTreeView::keyPressEvent(QKeyEvent* event)
{
   if (event->key() == Qt::Key_Delete)
   {
      QModelIndex currentIndex = selectionModel()->currentIndex();
      if (currentIndex.isValid())
      {
         auto* model = qobject_cast<SignalTreeModel*>(this->model());
         if (model)
         {
            QModelIndexList indexes = selectionModel()->selectedIndexes();
            model->RemoveSignals(indexes); // Удаляем выбранный сигнал
         }
      }
   }
   else
   {
      QTreeView::keyPressEvent(event);
   }
}

void
SignalTreeView::OnCustomMenuRequested(const QPoint& pos)
{
   QModelIndex index = this->indexAt(pos);
   if (index.isValid())
   {
      QMenu contextMenu;

      QAction* deleteAction = contextMenu.addAction("Delete");
      connect(deleteAction, &QAction::triggered, this, [index, this]()
      {
         auto* model = qobject_cast<SignalTreeModel*>(this->model());

         if (model)
         {
            QModelIndexList indexes = selectionModel()->selectedIndexes();
            model->RemoveSignals(indexes); // Удаляем выбранный сигнал
         }
      });
      contextMenu.exec(this->viewport()->mapToGlobal(pos));
   }
}

void
SignalTreeView::onItemExpanded(const QModelIndex& index)
{
   emitPinDescriptionSignal(index, true);
}

void
SignalTreeView::onItemCollapsed(const QModelIndex& index)
{
   emitPinDescriptionSignal(index, false);
}

void
SignalTreeView::emitPinDescriptionSignal(const QModelIndex& index, bool isExpanded)
{
   auto* model = qobject_cast<SignalTreeModel*>(this->model());
   if (model)
   {
      newVcd::PinDescriptionPtr pin = model->GetPinDescriptionFromIndex(index);
      if (pin)
      {
         emit itemExpandedOrCollapsed(pin, isExpanded);
      }
   }
}
