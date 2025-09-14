#include "TreeModulesModel.hpp"
#include <QMetaType>
#include <QModelIndex>
#include <QVariant>

/**************************************
 *            ModuleItem
 **************************************/

ModuleItem::ModuleItem(const std::shared_ptr<vcd::Module> &module, ModuleItem *parentItem)
    : m_module(module), m_parentItem(parentItem)
{
}

ModuleItem::~ModuleItem()
{
   qDeleteAll(m_childItems);
}

void ModuleItem::AppendChild(ModuleItem *child)
{
   m_childItems.append(child);
}

ModuleItem *
ModuleItem::Child(int row)
{
   return m_childItems.value(row);
}

std::size_t
ModuleItem::ChildCount() const
{
   return m_childItems.count();
}

int32_t
ModuleItem::Row() const
{
   if (m_parentItem)
   {
      return m_parentItem->m_childItems.indexOf(const_cast<ModuleItem *>(this));
   }
   return 0;
}

int32_t
ModuleItem::ColumnCount() const
{
   return 1;
}

QVariant
ModuleItem::Data(int column) const
{
   if (column == 0)
   {
      return QString::fromStdString(std::string(m_module->GetName()));
   }
   return QVariant();
}

ModuleItem *
ModuleItem::ParentItem()
{
   return m_parentItem;
}

const std::shared_ptr<vcd::Module> &
ModuleItem::GetModule() const
{
   return m_module;
}

/**************************************
 *         ModuleTreeModel
 **************************************/

ModuleTreeModel::ModuleTreeModel(QObject *parent)
    : QAbstractItemModel(parent), m_rootItem(nullptr)
{
}

ModuleTreeModel::~ModuleTreeModel()
{
   delete m_rootItem;
}

QVariant
ModuleTreeModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid() || role != Qt::DisplayRole)
      return QVariant();

   ModuleItem *item = static_cast<ModuleItem *>(index.internalPointer());
   return item->Data(index.column());
}

Qt::ItemFlags
ModuleTreeModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;

   return QAbstractItemModel::flags(index);
}

QVariant
ModuleTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
   {
      return QString("Modules");
   }
   return QVariant();
}

QModelIndex
ModuleTreeModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent))
      return QModelIndex();

   ModuleItem *parentItem;

   if (!parent.isValid())
   {
      parentItem = m_rootItem;
   }
   else
   {
      parentItem = static_cast<ModuleItem *>(parent.internalPointer());
   }

   ModuleItem *childItem = parentItem ? parentItem->Child(row) : nullptr;
   if (childItem)
   {
      return createIndex(row, column, childItem);
   }
   return QModelIndex();
}

QModelIndex
ModuleTreeModel::parent(const QModelIndex &index) const
{
   if (!index.isValid())
      return QModelIndex();

   ModuleItem *childItem = static_cast<ModuleItem *>(index.internalPointer());
   ModuleItem *parentItem = childItem ? childItem->ParentItem() : nullptr;

   if (parentItem == m_rootItem || parentItem == nullptr)
      return QModelIndex();

   return createIndex(parentItem->Row(), 0, parentItem);
}

int ModuleTreeModel::rowCount(const QModelIndex &parent) const
{
   ModuleItem *parentItem;
   if (!parent.isValid())
   {
      parentItem = m_rootItem;
   }
   else
   {
      parentItem = static_cast<ModuleItem *>(parent.internalPointer());
   }
   if (nullptr == parentItem)
   {
      return 0;
   }
   return static_cast<int>(parentItem->ChildCount());
}

int ModuleTreeModel::columnCount(const QModelIndex &parent) const
{
   Q_UNUSED(parent)
   return 1;
}

std::shared_ptr<vcd::Module>
ModuleTreeModel::GetModuleByIndex(const QModelIndex &index) const
{
   if (!index.isValid())
      return nullptr;

   ModuleItem *item = static_cast<ModuleItem *>(index.internalPointer());
   return item->GetModule();
}

void ModuleTreeModel::OnItemClicked(const QModelIndex &index)
{
   std::shared_ptr<vcd::Module> module = GetModuleByIndex(index);
   if (module)
   {
      emit ModuleClicked(module);
   }
}

void ModuleTreeModel::SetHandle(std::shared_ptr<vcd::Handle> VcdHandle)
{
   beginResetModel();

   if (m_rootItem)
   {
      delete m_rootItem;
      m_rootItem = nullptr;
   }

   m_handle = VcdHandle;
   m_rootItem = new ModuleItem(std::make_shared<vcd::Module>());
   if (m_handle && m_handle->GetRootModule())
   {
      SetupModelData({m_handle->GetRootModule()}, m_rootItem);
   }

   endResetModel();
}

void ModuleTreeModel::SetupModelData(const std::vector<std::shared_ptr<vcd::Module>> &modules, ModuleItem *parent)
{
   for (const auto &module : modules)
   {
      ModuleItem *moduleItem = new ModuleItem(module, parent);
      parent->AppendChild(moduleItem);
      if (!module->subModules().empty())
      {
         SetupModelData(module->subModules(), moduleItem);
      }
   }
}
