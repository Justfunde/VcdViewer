#ifndef __VCD_STRUCTURES_HPP__
#define __VCD_STRUCTURES_HPP__

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace newVcd
{
//======================================================================
// 1.  Общие enum-ы
//======================================================================
enum class PinType : std::uint8_t
{
   wire = 1,
   reg,
   parameter
};
enum class SigType : std::uint8_t
{
   simple = 1,
   complex
};

//======================================================================
// 2.  Вперёд-объявления
//======================================================================
class Handle;    // главный «контейнер» VCD-данных
class VcdReader; // парсер (header + body 1/МП)
class IPinDescription;
class SimplePinDescription;
class MultiplePinDescription;
class ParamPinDescription;

//======================================================================
// 3.  Контейнер «изменение сигнала»
//======================================================================
struct PinValue
{
   std::uint64_t timestamp {}; //!< момент изменения
   std::string_view value;     //!< view в mmap-буфере (для многобитовых)
};

//======================================================================
// 4.  Базовый класс pin-описаний + виртуальные getters
//======================================================================
class IPinDescription
{
 protected:
   // === поля, которые будут инициализироваться в конструкторе-делегате ===
   PinType m_pinType {}; //!< wire/reg/parameter
   SigType m_sigType {}; //!< simple/complex
   std::string m_alias;  //!< символьное имя в VCD ($var … alias)
   std::string m_name;   //!< human-readable name (обычно instance/pin)
   std::string m_initState;

   // --- конструктор-делегат, доступен только производным и friend'ам ---
   IPinDescription(PinType ptype, SigType stype,
                   std::string_view alias, std::string_view name) noexcept
      : m_pinType(ptype)
      , m_sigType(stype)
      , m_alias(alias)
      , m_name(name)
   {
   }

   // доступ к protected-полям имеют только Handle и VcdReader
   friend class Handle;
   friend class VcdReader;

 public:
   virtual ~IPinDescription() = default;

   //---------------- базовые геттеры ----------------
   PinType
   pinType() const noexcept
   {
      return m_pinType;
   }
   SigType
   sigType() const noexcept
   {
      return m_sigType;
   }
   std::string_view
   alias() const noexcept
   {
      return m_alias;
   }
   std::string_view
   name() const noexcept
   {
      return m_name;
   }

   void
   SetInitState(std::string_view state)
   {
      m_initState = state;
   }

   std::string
   initState() const noexcept
   {
      return m_initState;
   }

   //---------------- разделённые виртуальные getters ----------------
   /**  Для 1-битовых пинов. Возвращает символ '0','1','x' или 'z'. */
   virtual char getValueChar(std::uint64_t ts, std::size_t bit = 0) const = 0;

   /**  Для многобитовых пинов (bus) или параметров.
    *   Возвращает view (например, "1010" или "3F"). */
   virtual std::string_view getValueBus(std::uint64_t ts) const = 0;
};
using PinDescriptionPtr = std::shared_ptr<IPinDescription>;

//======================================================================
// 5.  Пины-параметры (const)
//======================================================================
class ParamPinDescription final : public IPinDescription
{
   // конструктор-делегат: PinType::parameter, SigType::simple, alias и name передаются сверху

   friend class VcdReader;
   friend class Handle;

 public:
   ParamPinDescription(std::string_view alias,
                       std::string_view name) noexcept
      : IPinDescription(PinType::parameter, SigType::simple, alias, name)
   {
   }

   std::string_view
   value() const noexcept
   {
      return m_initState;
   }

   // getValueChar для параметров можно считать «псевдо-символом»:
   char
   getValueChar(std::uint64_t /*ts*/, std::size_t /*bit*/ = 0) const override
   {
      // если нужно вернуть конкретный символ, можно, например, взять m_value[0],
      // но, поскольку параметр бывает многобитовый, обычно вызывают getValueBus.
      return m_initState.empty() ? '0' : m_initState.front();
   }

   std::string_view
   getValueBus(std::uint64_t /*ts*/) const override
   {
      return m_initState;
   }
};

//======================================================================
// 6.  Обычный 1-битовый pin
//======================================================================
class SimplePinDescription final : public IPinDescription
{
   std::vector<PinValue> m_values; //!< вектор изменений ‘0/1/x/z’ (ts + view на один символ)

   friend class VcdReader;
   friend class Handle;

 public:
   // конструктор-делегат: PinType::wire или PinType::reg, SigType::simple
   SimplePinDescription(PinType ptype, std::string_view alias, std::string_view name) noexcept
      : IPinDescription(ptype, SigType::simple, alias, name)
   {
   }

   const std::vector<PinValue>&
   timeline() const noexcept
   {
      return m_values;
   }

   // бинарный поиск: находим наибольшее ts' ≤ искомого и возвращаем значение
   char
   getValueChar(std::uint64_t ts, std::size_t /*bit*/ = 0) const override
   {
      // простейшая реализация: линейный/бинарный поиск по m_values
      if (m_values.empty())
         return '0';
      std::size_t lo = 0, hi = m_values.size() - 1;
      // стандартный std::lower_bound с проверкой
      while (lo < hi)
      {
         std::size_t mid = (lo + hi + 1) / 2;
         if (m_values[mid].timestamp <= ts)
            lo = mid;
         else
            hi = mid - 1;
      }
      return m_values[lo].value.empty() ? '0' : m_values[lo].value.front();
   }

   // Для 1-битового пина возврат bus-строки бессмысленен; возвращаем строку из одного символа.
   std::string_view
   getValueBus(std::uint64_t ts) const override
   {
      static thread_local std::string tmp; // делаем временную строку, чтобы вернуть std::string_view
      char c = getValueChar(ts);
      tmp.assign(1, c);
      return tmp;
   }
};

//======================================================================
// 7.  Многобитовая шина / комплексный пин
//======================================================================
class MultiplePinDescription final : public IPinDescription
{
   std::pair<std::size_t, std::size_t> m_bitDepth;               //!< {msb, lsb}
   std::vector<PinValue> m_values;                               //!< строки «1010…» (ts + view)
   std::vector<std::shared_ptr<SimplePinDescription>> m_subpins; //!< опционально, для битовых обращений

   // конструктор-делегат: PinType::wire/PinType::reg, SigType::complex

   friend class VcdReader;
   friend class Handle;

 public:
   MultiplePinDescription(PinType ptype, std::string_view alias, std::string_view name,
                          std::pair<std::size_t, std::size_t> bitDepth) noexcept
      : IPinDescription(ptype, SigType::complex, alias, name)
      , m_bitDepth(bitDepth)
   {
   }

   std::pair<std::size_t, std::size_t>
   bitDepth() const noexcept
   {
      return m_bitDepth;
   }
   const std::vector<PinValue>&
   timeline() const noexcept
   {
      return m_values;
   }
   const std::vector<std::shared_ptr<SimplePinDescription>>&
   subPins() const noexcept
   {
      return m_subpins;
   }

   // Возвращаем всю строку «1010…» как view
   std::string_view
   getValueBus(std::uint64_t ts) const override
   {
      if (m_values.empty())
         return std::string_view {};
      // поиск ближайшего ts ≤ заданного
      std::size_t lo = 0, hi = m_values.size() - 1;
      while (lo < hi)
      {
         std::size_t mid = (lo + hi + 1) / 2;
         if (m_values[mid].timestamp <= ts)
            lo = mid;
         else
            hi = mid - 1;
      }
      return m_values[lo].value;
   }

   // Для доступа к отдельному биту (через subPins или напрямую из строки)
   char
   getValueChar(std::uint64_t ts, std::size_t bit = 0) const override
   {
      // 1) Если есть m_subpins, можно обратиться к нужному subpin:
      if (!m_subpins.empty())
      {
         auto idx = bit; // битовый номер (0 = LSB, bitDepth-1 = MSB), уточните логику
         if (idx < m_subpins.size())
            return m_subpins[idx]->getValueChar(ts);
      }
      // 2) Иначе, возвращаем символ из строки bus
      auto bus = getValueBus(ts);
      if (bus.empty())
         return '0';
      // Предполагаем, что bus[0] = MSB, bus.back() = LSB
      std::size_t strIdx = (bus.size() > bit) ? (bus.size() - 1 - bit) : 0;
      return bus[strIdx];
   }
};

//======================================================================
// 8.  Дерево модулей
//======================================================================
class Module
{
 private:
   std::string m_moduleName;
   std::vector<PinDescriptionPtr> m_pins;
   std::vector<std::shared_ptr<Module>> m_subModules;

   friend class VcdReader;
   friend class Handle;

 public:
   std::string_view
   name() const noexcept
   {
      return m_moduleName;
   }
   const std::vector<std::shared_ptr<Module>>&
   subModules() const noexcept
   {
      return m_subModules;
   }

   std::size_t
   GetsubModulesCnt() const noexcept
   {
      return m_subModules.size();
   }

   const std::vector<PinDescriptionPtr>&
   GetPins() const noexcept
   {
      return m_pins;
   }
};

//======================================================================
// 9.  Главный объект VCD-файла
//======================================================================
class Handle
{
   //-------------------------------------------- друзья
   friend class VcdReader;

   //-------------------------------------------- метаданные
   std::string m_date;
   std::string m_version;
   std::string m_timescale;
   std::uint64_t m_maxTimestamp {0};

   std::shared_ptr<Module> m_root;
   std::vector<PinDescriptionPtr> m_pins;
   std::unordered_map<std::string_view, PinDescriptionPtr> m_alias2pin;

   std::filesystem::path m_filepath;
   int m_fd {-1};
   const char* m_dataPtr {nullptr};

   std::queue<std::string> m_tokens;
   std::size_t m_size {0};
   std::size_t m_tsOffset {0};

   std::size_t m_chunkSize = 0; //!< 0 ⇒ mmap всего файла, иначе разбивка на куски по m_chunkSize.

 public:
   //-------------------------------------------- ctor/dtor
   Handle() = default;

   ~Handle(); //!< unmap + close
   Handle(const Handle&) = delete;
   Handle& operator=(const Handle&) = delete;
   Handle(Handle&&) noexcept;
   Handle& operator=(Handle&&) noexcept;

   void
   Init(const std::filesystem::path& fileName);

   void
   LoadHdr();

   void
   LoadSignals();

   void
   LoadSignalsParallel();

   //-------------------------------------------- info
   std::string_view
   date() const noexcept
   {
      return m_date;
   }

   std::string_view
   version() const noexcept
   {
      return m_version;
   }

   std::string_view
   timeScale() const noexcept
   {
      return m_timescale;
   }

   std::uint64_t
   maxTs() const noexcept
   {
      return m_maxTimestamp;
   }

   auto&
   alias2pinMap() const noexcept
   {
      return m_alias2pin;
   }

   std::shared_ptr<Module>
   root() const noexcept
   {
      return m_root;
   }

   const std::vector<PinDescriptionPtr>&
   pins() const noexcept
   {
      return m_pins;
   }

   //-------------------------------------------- lookup
   PinDescriptionPtr
   pinByAlias(std::string_view a) const
   {
      auto it = m_alias2pin.find(a);
      return it == m_alias2pin.end() ? nullptr : it->second;
   }

   //-------------------------------------------- value getters (proxy)
   char
   getValueChar(std::uint64_t ts, std::string_view alias, std::size_t bit = 0) const
   {
      auto pin = pinByAlias(alias);
      if (!pin)
         return '0';
      return pin->getValueChar(ts, bit);
   }

   std::string_view
   getValueBus(std::uint64_t ts, std::string_view alias) const
   {
      auto pin = pinByAlias(alias);
      if (!pin)
         return {};
      return pin->getValueBus(ts);
   }

 private:
   std::queue<std::string>
   Tokenize(std::string_view fileData);

   std::string
   ExtractDate();

   std::string
   ExtractVersion();

   std::string
   ExtractTimescale();

   std::shared_ptr<Module>
   ExtractScope();

   PinDescriptionPtr
   ExtractVar();

   void
   ExtractEndDefinitions();

   void
   ExtractComment();

   void
   ExtractDumpAll();

   std::vector<std::pair<std::string, std::string>>
   ExtractDumpVars();

   void
   FillInitStates(const std::vector<std::pair<std::string, std::string>>& dumpVars);
};

} // namespace newVcd

#endif //!__VCD_STRUCTURES_HPP__