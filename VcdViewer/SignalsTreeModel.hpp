#pragma once

#include "Include/VcdStructs.hpp"
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPair>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>
#include <QVector>
#include <memory>
#include <vector>

struct EmmitedSignalDescription
{
  vcd::PinDescriptionPtr pinDescription;
  QString name;
  QString value;
};

/**
 * @brief Модель данных для отображения сигналов VCD-файла в древовидной структуре.
 *
 * Модель поддерживает верхний уровень сигналов и при наличии битовой глубины - дочерние элементы,
 * соответствующие отдельным битам сигнала.
 */
class SignalTreeModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  /**
   * @brief Конструктор модели сигналов.
   * @param parent Родительский QObject.
   */
  explicit SignalTreeModel(QObject *parent = nullptr);

  /**
   * @brief Установить список сигналов для отображения.
   * @param signalVector Вектор сигналов.
   */
  void
  SetSignals(const std::vector<vcd::PinDescriptionPtr> &signalVector);

  /**
   * @brief Получить описание сигнала (PinDescription) по индексу.
   * @param index Индекс в модели.
   * @return Указатель на PinDescription или nullptr.
   */
  vcd::PinDescriptionPtr
  GetPinDescriptionFromIndex(const QModelIndex &index) const;

signals:
  /**
   * @brief Сигнал, испускаемый при изменении списка сигналов.
   * @param Список сигналов.
   */
  void SignalListChanged(std::vector<vcd::PinDescriptionPtr>);

  void SignalsUpdated(QVector<EmmitedSignalDescription>);

public slots:
  /**
   * @brief Обновить все данные в модели, перерисовав отображение.
   * @param parent Родительский индекс, по умолчанию корневой.
   */
  void
  UpdateAllData(const QModelIndex &parent = QModelIndex());

  /**
   * @brief Установить выбранный временной срез.
   * @param ts Временная метка.
   */
  void
  SetSelectedTimestamp(uint64_t ts);

  /**
   * @brief Добавить сигнал в список.
   * @param sig Описание сигнала.
   */
  void
  AppendSignal(vcd::PinDescriptionPtr sig);

  void
  AppendSignals(const std::vector<vcd::PinDescriptionPtr> &sigs);

  void
  ReplaceSignalsWithSelection(
      QItemSelectionModel *selectionModel,
      const std::vector<vcd::PinDescriptionPtr> &newSignals);

  void
  InsertSignals(
      const std::vector<vcd::PinDescriptionPtr> &sigs,
      std::size_t pos);

  /**
   * @brief Удалить сигнал из списка по индексу.
   * @param index Индекс сигнала для удаления.
   */
  void RemoveSignal(const QModelIndex &index);

  void
  RemoveSignals(const QModelIndexList &idxs);

  /**
   * @brief Установить новый VCD дескриптор и очистить список сигналов.
   * @param VcdHandle Дескриптор VCD.
   */
  void
  SetHandle(std::shared_ptr<vcd::Handle> VcdHandle);

  /**
   * @brief Возвращает количество строк (дочерних элементов) для заданного индекса.
   * @param parent Родительский индекс.
   * @return Количество строк.
   */
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Возвращает количество столбцов (всегда 1).
   * @param parent Родительский индекс.
   * @return Количество столбцов.
   */
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Возвращает данные для отображения в представлении.
   * @param index Индекс модели.
   * @param role Роль данных (по умолчанию Qt::DisplayRole).
   * @return Данные для отображения.
   */
  QVariant
  data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

  /**
   * @brief Возвращает данные заголовка модели.
   * @param section Номер столбца.
   * @param orientation Ориентация (горизонтальная/вертикальная).
   * @param role Роль данных.
   * @return Данные заголовка.
   */
  QVariant
  headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  /**
   * @brief Возвращает индекс по строке, столбцу и родительскому индексу.
   * @param row Номер строки.
   * @param column Номер столбца.
   * @param parent Родительский индекс.
   * @return Индекс модели.
   */
  QModelIndex
  index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

  /**
   * @brief Возвращает родительский индекс для данного индекса.
   * @param index Индекс модели.
   * @return Родительский индекс.
   */
  QModelIndex
  parent(const QModelIndex &index) const override;

private:
  /**
   * @brief Получить ближайший доступный временной штамп, не превышающий заданный.
   * @param value Запрашиваемый временной штамп.
   * @return Выбранный временной штамп из набора доступных.
   */
  uint64_t
  GetSelectedTimestamp(uint64_t value) const;

  /**
   * @brief Получить значение сигнала на определённом временном штампе.
   * @param pin Описание сигнала.
   * @param index Индекс модели.
   * @param timestamp Временной штамп.
   * @return Значение сигнала или пустой std::optional, если значение недоступно.
   */
  std::optional<QString>
  GetPinValueAtTimestamp(vcd::PinDescriptionPtr pin, const QModelIndex &index, uint64_t timestamp) const;

  void
  emitSignalsAndValues();

  std::vector<vcd::PinDescriptionPtr> m_signals; ///< Список отображаемых сигналов.
  std::optional<uint64_t> m_timestamp;           ///< Текущий выбранный временной штамп.
  std::shared_ptr<vcd::Handle> m_handle;         ///< Дескриптор VCD.
};

/**
 * @brief Делегат для установки фиксированной высоты строк в QTreeView.
 */
class FixedHeightDelegate : public QStyledItemDelegate
{
public:
  /**
   * @brief Конструктор делегата с фиксированной высотой.
   * @param fixedHeight Фиксированная высота для строк.
   * @param parent Родительский объект.
   */
  explicit FixedHeightDelegate(int fixedHeight, QObject *parent = nullptr);

  /**
   * @brief Возвращает рекомендуемый размер элемента с учётом фиксированной высоты.
   * @param option Опции стиля.
   * @param index Индекс модели.
   * @return Размер элемента.
   */
  QSize
  sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
  int m_fixedHeight; ///< Фиксированная высота строки.
};

/**
 * @brief Виджет для отображения сигналов в виде дерева.
 *
 * Поддерживает удаление выбранных сигналов по клавише Delete и генерирует сигналы при
 * разворачивании/свёртывании узлов.
 */
class SignalTreeView : public QTreeView
{
  Q_OBJECT

public:
  /**
   * @brief Конструктор представления.
   * @param parent Родительский QWidget.
   */
  explicit SignalTreeView(QWidget *parent = nullptr);

  /**
   * @brief Получить вертикальный скроллбар представления.
   * @return Указатель на QScrollBar.
   */
  QScrollBar *
  GetVerticalScrollBar() const;

signals:
  /**
   * @brief Сигнал, испускаемый при разворачивании или сворачивании элемента.
   * @param pin Указатель на сигнал.
   * @param isExpanded Флаг, указывающий на состояние (развернут или свернут).
   */
  void itemExpandedOrCollapsed(vcd::PinDescriptionPtr pin, bool isExpanded);

protected:
  /**
   * @brief Обработчик нажатия клавиш.
   *
   * Нажатие клавиши Delete удаляет выбранный сигнал.
   * @param event Событие нажатия клавиши.
   */
  void
  keyPressEvent(QKeyEvent *event) override;
private slots:
  void
  onItemExpanded(const QModelIndex &index);

  void
  onItemCollapsed(const QModelIndex &index);

  void
  OnCustomMenuRequested(const QPoint &pos);

private:
  /**
   * @brief Вспомогательный метод для генерации сигнала о раскрытии/свёртывании.
   * @param index Индекс элемента.
   * @param isExpanded Флаг раскрытия.
   */
  void
  emitPinDescriptionSignal(const QModelIndex &index, bool isExpanded);

private:
};

Q_DECLARE_METATYPE(std::vector<vcd::PinDescriptionPtr>)
