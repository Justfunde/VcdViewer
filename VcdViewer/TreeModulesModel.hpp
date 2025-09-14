#pragma once

#include <QAbstractItemModel>
#include <QTreeView>
#include <QVariant>
#include <memory>

#include "Include/VcdStructs.hpp"

/**
 * @brief Класс, представляющий элемент дерева модулей.
 *
 * Каждый элемент соответствует конкретному модулю из VCD.
 */
class ModuleItem
{
public:
  /**
   * @brief Конструктор элемента модуля.
   * @param module Указатель на структуру модуля.
   * @param parentItem Родительский элемент, если имеется.
   */
  explicit ModuleItem(const std::shared_ptr<vcd::Module> &module, ModuleItem *parentItem = nullptr);

  /**
   * @brief Деструктор элемента модуля.
   */
  ~ModuleItem();

  /**
   * @brief Добавить дочерний элемент к текущему.
   * @param child Указатель на дочерний элемент.
   */
  void
  AppendChild(ModuleItem *child);

  /**
   * @brief Получить дочерний элемент по индексу.
   * @param row Индекс дочернего элемента.
   * @return Указатель на дочерний элемент или nullptr.
   */
  ModuleItem *
  Child(int row);

  /**
   * @brief Получить количество дочерних элементов.
   * @return Число дочерних элементов.
   */
  std::size_t
  ChildCount() const;

  /**
   * @brief Получить индекс (строку) текущего элемента относительно родителя.
   * @return Индекс элемента.
   */
  int32_t
  Row() const;

  /**
   * @brief Получить количество столбцов (в данном случае всегда 1).
   * @return Количество столбцов.
   */
  int32_t
  ColumnCount() const;

  /**
   * @brief Получить данные для отображения в заданном столбце.
   * @param column Номер столбца.
   * @return QVariant с данными для отображения (имя модуля в столбце 0).
   */
  QVariant
  Data(int column) const;

  /**
   * @brief Получить родительский элемент.
   * @return Указатель на родительский элемент или nullptr.
   */
  ModuleItem *
  ParentItem();

  /**
   * @brief Получить указатель на соответствующий модуль.
   * @return Константная ссылка на указатель std::shared_ptr<vcdModule>.
   */
  const std::shared_ptr<vcd::Module> &
  GetModule() const;

private:
  std::shared_ptr<vcd::Module> m_module; ///< Указатель на модуль.
  QList<ModuleItem *> m_childItems;      ///< Список дочерних элементов.
  ModuleItem *m_parentItem;              ///< Родительский элемент.
};

/**
 * @brief Модель данных для дерева модулей, основанная на QAbstractItemModel.
 *
 * Данный класс отображает иерархию модулей, полученных из VCD-файла.
 * Он поддерживает клики по элементам, позволяя обработать выбор конкретного модуля.
 */
class ModuleTreeModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  /**
   * @brief Конструктор модели.
   * @param parent Родительский объект.
   */
  explicit ModuleTreeModel(QObject *parent = nullptr);

  /**
   * @brief Деструктор модели.
   */
  ~ModuleTreeModel() override;

  /**
   * @brief Возвращает данные для отображения.
   * @param index Индекс модели.
   * @param role Роль данных (по умолчанию Qt::DisplayRole).
   * @return QVariant с данными для отображения.
   */
  QVariant
  data(const QModelIndex &index, int role) const override;

  /**
   * @brief Возвращает флаги элемента по индексу.
   * @param index Индекс модели.
   * @return Флаги элемента.
   */
  Qt::ItemFlags
  flags(const QModelIndex &index) const override;

  /**
   * @brief Заголовочные данные модели.
   * @param section Номер секции (столбца).
   * @param orientation Ориентация (горизонтальная или вертикальная).
   * @param role Роль данных.
   * @return Данные заголовка.
   */
  QVariant
  headerData(int section, Qt::Orientation orientation, int role) const override;

  /**
   * @brief Получить индекс элемента модели по строке, столбцу и родительскому индексу.
   * @param row Строка.
   * @param column Столбец.
   * @param parent Родительский индекс.
   * @return Индекс в модели.
   */
  QModelIndex
  index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Получить родительский индекс для данного индекса.
   * @param index Индекс модели.
   * @return Родительский индекс.
   */
  QModelIndex
  parent(const QModelIndex &index) const override;

  /**
   * @brief Возвращает количество строк (дочерних элементов) для заданного родителя.
   * @param parent Родительский индекс.
   * @return Количество дочерних элементов.
   */
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Возвращает количество столбцов модели.
   * @param parent Родительский индекс (игнорируется, т.к. всегда 1 столбец).
   * @return Количество столбцов.
   */
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Получить модуль по индексу модели.
   * @param index Индекс модели.
   * @return Указатель на модуль или nullptr.
   */
  std::shared_ptr<vcd::Module>
  GetModuleByIndex(const QModelIndex &index) const;

signals:
  /**
   * @brief Сигнал, испускаемый при клике на модуль.
   * @param module Указатель на выбранный модуль.
   */
  void ModuleClicked(std::shared_ptr<vcd::Module> module);

public slots:
  /**
   * @brief Слот, вызываемый при клике на элемент дерева.
   * @param index Индекс элемента.
   */
  void
  OnItemClicked(const QModelIndex &index);

  /**
   * @brief Установить новый VCD дескриптор (Handle) и обновить дерево модулей.
   * @param VcdHandle Новый дескриптор.
   */
  void
  SetHandle(std::shared_ptr<vcd::Handle> VcdHandle);

private:
  /**
   * @brief Рекурсивный метод для заполнения модели данными из списка модулей.
   * @param modules Вектор модулей.
   * @param parent Родительский элемент.
   */
  void
  SetupModelData(const std::vector<std::shared_ptr<vcd::Module>> &modules, ModuleItem *parent);

  ModuleItem *m_rootItem;                ///< Корневой элемент дерева модулей.
  std::shared_ptr<vcd::Handle> m_handle; ///< Текущий дескриптор VCD.
};

Q_DECLARE_METATYPE(std::shared_ptr<vcd::Module>)
