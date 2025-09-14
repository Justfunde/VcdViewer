#ifndef __VCD_STRUCTURES_HPP__
#define __VCD_STRUCTURES_HPP__

#include <filesystem>
#include <algorithm>
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
#include <iostream>

namespace vcd
{
   class Module;
   //======================================================================
   // 1.  Общие enum-ы
   //======================================================================
   enum class PinType : std::uint8_t
   {
      wire = 1,
      reg,
      parameter
   };
   enum class SignalType : std::uint8_t
   {
      simple = 1,
      bus
   };

   //======================================================================
   // 2.  Вперёд-объявления
   //======================================================================
   class Handle;    // главный «контейнер» VCD-данных
   class VcdReader; // парсер (header + body 1/МП)
   class IPinDescription;
   class SimplePinDescription;
   class BusPinDescription;
   class ParamPinDescription;

   //======================================================================
   // 3.  Контейнер «изменение сигнала»
   //======================================================================
   struct PinValue
   {
      std::uint64_t timestamp{}; //!< момент изменения
      std::string_view value;    //!< view в mmap-буфере (для многобитовых)
   };

   //======================================================================
   // 4.  Базовый класс pin-описаний + виртуальные getters
   //======================================================================
   class IPinDescription
   {
   protected:
      // === поля, которые будут инициализироваться в конструкторе-делегате ===
      PinType m_pinType{};    //!< wire/reg/parameter
      SignalType m_sigType{}; //!< simple/bus
      std::string m_alias;    //!< символьное имя в VCD ($var … alias)
      std::string m_name;     //!< human-readable name (обычно instance/pin)
      std::string m_initState;

      std::weak_ptr<Module> m_parent;

      // --- конструктор-делегат, доступен только производным и friend'ам ---
      IPinDescription(PinType ptype, SignalType stype,
                      std::string_view alias, std::string_view name) noexcept
          : m_pinType(ptype), m_sigType(stype), m_alias(alias), m_name(name)
      {
      }

      // доступ к protected-полям имеют только Handle и VcdReader
      friend class Handle;
      friend class VcdReader;

   public:
      virtual ~IPinDescription() = default;

      //---------------- базовые геттеры ----------------
      PinType
      GetPinType() const noexcept
      {
         return m_pinType;
      }

      SignalType
      GetSignalType() const noexcept
      {
         return m_sigType;
      }

      std::string_view
      GetAlias() const noexcept
      {
         return m_alias;
      }

      std::string_view
      GetName() const noexcept
      {
         return m_name;
      }

      void
      SetInitState(std::string_view state)
      {
         m_initState = state;
      }

      virtual std::string
      GetInitState() const noexcept
      {
         return m_initState;
      }

      std::weak_ptr<Module>
      GetParent()
      {
         return m_parent;
      }

      void SetParent(std::weak_ptr<Module> parent)
      {
         m_parent = parent;
      }

      //---------------- разделённые виртуальные getters ----------------
      /**  Для 1-битовых пинов. Возвращает символ '0','1','x' или 'z'. */
      virtual char GetValueChar(std::uint64_t ts, std::size_t bit = 0) const = 0;

      /**  Для многобитовых пинов (bus) или параметров.
       *   Возвращает view (например, "1010"). */
      virtual std::string_view GetValueBus(std::uint64_t ts) const = 0;
   };
   using PinDescriptionPtr = std::shared_ptr<IPinDescription>;

   //======================================================================
   // 5.  Пины-параметры (const)
   //======================================================================
   class ParamPinDescription final : public IPinDescription
   {
      // конструктор-делегат: PinType::parameter, SignalType::simple, alias и name передаются сверху

      friend class VcdReader;
      friend class Handle;

   public:
      ParamPinDescription(std::string_view alias,
                          std::string_view name) noexcept
          : IPinDescription(PinType::parameter, SignalType::simple, alias, name)
      {
      }

      std::string_view
      GetValue() const noexcept
      {
         return m_initState;
      }

      // GetValueChar для параметров можно считать «псевдо-символом»:
      char
      GetValueChar(std::uint64_t /*ts*/, std::size_t /*bit*/ = 0) const override
      {
         // если нужно вернуть конкретный символ, можно, например, взять m_value[0],
         // но, поскольку параметр бывает многобитовый, обычно вызывают GetValueBus.
         return m_initState.empty() ? '0' : m_initState.front();
      }

      std::string_view
      GetValueBus(std::uint64_t /*ts*/) const override
      {
         return m_initState;
      }
   };

   //======================================================================
   // 6.  Обычный 1-битовый pin
   //======================================================================
   class SimplePinDescription : public IPinDescription
   {
   public:
      // конструктор-делегат: PinType::wire или PinType::reg, SignalType::simple
      SimplePinDescription(PinType ptype, std::string_view alias, std::string_view name) noexcept
          : IPinDescription(ptype, SignalType::simple, alias, name)
      {
      }

      virtual const std::vector<PinValue> &
      GetTimeline() const noexcept
      {
         return m_values;
      }

      // бинарный поиск: находим наибольшее ts' ≤ искомого и возвращаем значение
      char
      GetValueChar(std::uint64_t ts, std::size_t /*bit*/ = 0) const override
      {
         // простейшая реализация: линейный/бинарный поиск по m_values
         if (m_values.empty())
            return '0';
         if (ts < m_values[0].timestamp)
         {
            return GetInitState()[0];
         }

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
      GetValueBus(std::uint64_t ts) const override
      {
         static thread_local std::string tmp; // делаем временную строку, чтобы вернуть std::string_view
         char c = GetValueChar(ts);
         tmp.assign(1, c);
         return tmp;
      }

   private:
      std::vector<PinValue> m_values; //!< вектор изменений ‘0/1/x/z’ (ts + view на один символ)

      friend class VcdReader;
      friend class Handle;
      friend class MultiplePinDiscription;
   };

   //======================================================================
   // 7.  Многобитовая шина / комплексный пин
   //======================================================================
   class BusPinDescription final : public IPinDescription, public std::enable_shared_from_this<BusPinDescription>
   {

   public:
      BusPinDescription(PinType ptype, std::string_view alias, std::string_view name,
                        std::pair<std::size_t, std::size_t> bitDepth) noexcept
          : IPinDescription(ptype, SignalType::bus, alias, name), m_bitDepth(bitDepth)
      {
      }

      std::pair<std::size_t, std::size_t>
      GetBitDepth() const noexcept
      {
         return m_bitDepth;
      }

      const std::vector<PinValue> &
      GetTimeline() const noexcept
      {
         return m_values;
      }

      const std::vector<std::shared_ptr<SimplePinDescription>> &
      GetSubPins() const noexcept
      {

         if (m_subpins.empty())
         {
            auto self = shared_from_this();
            auto [msb, lsb] = GetBitDepth();
            std::size_t nBits = msb - lsb + 1;

            m_subpins.reserve(nBits);
            for (std::size_t i = 0; i < nBits; ++i)
               m_subpins.emplace_back(std::make_shared<BitProxy>(self));
         }
         return m_subpins;
      }

      // Возвращаем всю строку «1010…» как view
      std::string_view
      GetValueBus(std::uint64_t ts) const override
      {
         if (ts < m_values.begin()->timestamp)
         {
            return m_initState;
         }
         if (m_values.empty())
            return std::string_view{};
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
      GetValueChar(std::uint64_t ts, std::size_t bit = 0) const override
      {
         if (ts < m_values.begin()->timestamp)
         {
            return m_initState[0];
         }
         // 2) Иначе, возвращаем символ из строки bus
         auto bus = GetValueBus(ts);
         if (bus.empty())
            return '0';

         if (bus == "z" || bus == "x")
         {
            return bus[0];
         }

         if (bus.size() <= bit)
         {
            return '0';
         }
         else
         {
            return bus[bit];
         }
      }

   private:
      struct BitProxy : public SimplePinDescription
      {
         std::shared_ptr<const BusPinDescription> parent;

         BitProxy(std::shared_ptr<const BusPinDescription> p)
             : SimplePinDescription(p->m_pinType, "", ""),
               parent(std::move(p)) {}

         std::string
         GetInitState() const noexcept override
         {
            return parent->GetInitState();
         }

         char GetValueChar(uint64_t ts,
                           std::size_t bit) const override
         {
            // std::cout << "ts:" << ts << "bit" << bit << std::endl;
            return parent->GetValueChar(ts, bit);
         }

         const std::vector<PinValue> &GetTimeline() const noexcept override
         {
            return parent->GetTimeline();
         }
      };

   private:
      std::pair<std::size_t, std::size_t> m_bitDepth;                       //!< {msb, lsb}
      std::vector<PinValue> m_values;                                       //!< строки «1010…» (ts + view)
      mutable std::vector<std::shared_ptr<SimplePinDescription>> m_subpins; //!< опционально, для битовых обращений

      // конструктор-делегат: PinType::wire/PinType::reg, SignalType::bus

      friend class VcdReader;
      friend class Handle;
   };

   //======================================================================
   // 8.  Дерево модулей
   //======================================================================
   class Module
   {
   public:
      std::string_view
      GetName() const noexcept
      {
         return m_moduleName;
      }
      const std::vector<std::shared_ptr<Module>> &
      subModules() const noexcept
      {
         return m_subModules;
      }

      std::size_t
      GetsubModulesCnt() const noexcept
      {
         return m_subModules.size();
      }

      const std::vector<PinDescriptionPtr> &
      GetPins() const noexcept
      {
         return m_pins;
      }

      void SetParent(std::weak_ptr<Module> parent)
      {
         m_parent = parent;
      }

      std::weak_ptr<Module> GetParent()
      {
         return m_parent;
      }

   private:
      std::string m_moduleName;
      std::vector<PinDescriptionPtr> m_pins;
      std::vector<std::shared_ptr<Module>> m_subModules;
      std::weak_ptr<Module> m_parent;

      friend class VcdReader;
      friend class Handle;
   };

   //======================================================================
   // 9.  Главный объект VCD-файла
   //======================================================================
   class Handle
   {
      //-------------------------------------------- друзья
      friend class VcdReader;

   public:
      //-------------------------------------------- ctor/dtor
      Handle() = default;

      ~Handle(); //!< unmap + close
      Handle(const Handle &) = delete;
      Handle &operator=(const Handle &) = delete;
      Handle(Handle &&) noexcept;
      Handle &operator=(Handle &&) noexcept;

      void
      Init(const std::filesystem::path &fileName);

      void
      LoadHdr();

      void
      LoadSignals();

      void
      LoadSignalsParallel();

      //-------------------------------------------- info
      std::string_view
      GetDate() const noexcept
      {
         return m_date;
      }

      std::string_view
      GetVersion() const noexcept
      {
         return m_version;
      }

      std::string_view
      GetTimeScale() const noexcept
      {
         return m_timescale;
      }

      std::uint64_t
      GetMaxTs() const noexcept
      {
         return m_maxTimestamp;
      }

      auto &
      GetAlias2pinMap() const noexcept
      {
         return m_alias2pin;
      }

      std::shared_ptr<Module>
      GetRootModule() const noexcept
      {
         return m_root;
      }

      const std::vector<PinDescriptionPtr> &
      GetPins() const noexcept
      {
         return m_pins;
      }

      //-------------------------------------------- lookup
      PinDescriptionPtr
      GetPinByAlias(std::string_view a) const
      {
         auto it = m_alias2pin.find(a);
         return it == m_alias2pin.end() ? nullptr : it->second;
      }

      //-------------------------------------------- value getters (proxy)
      char
      GetValueChar(std::uint64_t ts, std::string_view alias, std::size_t bit = 0) const
      {
         auto pin = GetPinByAlias(alias);
         if (!pin)
            return '0';
         return pin->GetValueChar(ts, bit);
      }

      std::string_view
      GetValueBus(std::uint64_t ts, std::string_view alias) const
      {
         auto pin = GetPinByAlias(alias);
         if (!pin)
            return {};
         return pin->GetValueBus(ts);
      }

      std::vector<std::pair<uint64_t, uint64_t>>
      GetDumpoffIntervals() const
      {
         return m_dumpoffIntervals;
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
      FillInitStates(const std::vector<std::pair<std::string, std::string>> &dumpVars);

   private:
      void LinkParent(std::shared_ptr<Module> parent, const std::vector<std::shared_ptr<Module>> &childs);
      //-------------------------------------------- метаданные
      std::string m_date;
      std::string m_version;
      std::string m_timescale;
      std::uint64_t m_maxTimestamp{0};

      std::shared_ptr<Module> m_root;
      std::vector<PinDescriptionPtr> m_pins;
      std::unordered_map<std::string_view, PinDescriptionPtr> m_alias2pin;

      std::vector<std::pair<uint64_t, uint64_t>> m_dumpoffIntervals;

      std::filesystem::path m_filepath;
      std::string m_data;

      std::size_t m_size = 0;

      std::queue<std::string> m_tokens;
      std::size_t m_tsOffset{0};

      std::size_t m_chunkSize = 0; //!< 0 -> mmap всего файла, иначе разбивка на куски по m_chunkSize.
   };

} // namespace vcd

#endif //!__VCD_STRUCTURES_HPP__