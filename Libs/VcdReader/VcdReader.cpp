#include "Include/VcdStructs.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <fcntl.h>    // open
#include <sys/mman.h> // mmap, munmap
#include <unistd.h>   // close

namespace newVcd
{
   std::string
   Handle::ExtractDate()
   {
      std::string date;
      date.reserve(sizeof("Sat Oct 26 22:14:13 2024"));

      auto currToken = m_tokens.front();
      while (!m_tokens.empty() && currToken != "$end")
      {
         m_tokens.pop();
         date.append(currToken);
         date.append(" ");

         currToken = m_tokens.front();
      }
      date.pop_back();
      return date;
   }

   std::string
   Handle::ExtractVersion()
   {
      std::string version;
      auto currToken = m_tokens.front();
      while (!m_tokens.empty() && currToken != "$end")
      {
         m_tokens.pop();
         version.append(currToken);
         version.append(" ");

         currToken = m_tokens.front();
      }
      version.pop_back();
      return version;
   }

   std::string
   Handle::ExtractTimescale()
   {
      auto timeScale = m_tokens.front();
      m_tokens.pop();
      m_tokens.pop();
      return timeScale;
   }

   std::shared_ptr<Module>
   Handle::ExtractScope()
   {
      auto scopeName = m_tokens.front();
      m_tokens.pop();
      if (scopeName == "module")
      {
         std::shared_ptr<Module> module = std::make_shared<Module>();
         module->m_moduleName = m_tokens.front();
         m_tokens.pop(); // skip moduleName
         m_tokens.pop(); // skip end

         auto currentToken = m_tokens.front();
         while (currentToken != "$upscope")
         {
            m_tokens.pop();
            if (currentToken == "$var")
            {
               PinDescriptionPtr var = ExtractVar();
               if (m_alias2pin.count(var->alias()))
               {
                  module->m_pins.push_back(m_alias2pin[var->alias()]);
               }
               else
               {
                  module->m_pins.push_back(var);
                  m_alias2pin[var->alias()] = var;
               }
            }
            if (currentToken == "$scope")
            {
               if (auto submodule = ExtractScope(); submodule != nullptr)
               {
                  module->m_subModules.push_back(submodule);
               }
            }
            currentToken = m_tokens.front();
         }
         m_tokens.pop(); // skip upscope
         m_tokens.pop(); // skip end
         return module;
      }
      else if (scopeName == "begin")
      {
         m_tokens.pop(); // skip test
         m_tokens.pop(); // skip end

         std::string currentToken;
         while (currentToken != "$upscope")
         {
            currentToken = m_tokens.front();
            m_tokens.pop();
         }
         m_tokens.pop(); // skip upscope
         // tokens.pop(); // skip end
      }
      else
      {
         // assert(true);
      }
      return nullptr;
   }

   PinDescriptionPtr
   Handle::ExtractVar()
   {
      static std::unordered_map<std::string, PinType> strToPinTypeMap =
          {
              {"wire", PinType::wire},
              {"reg", PinType::reg},
              {"parameter", PinType::parameter}};

      PinDescriptionPtr descrPtr;
      // type
      const auto type = strToPinTypeMap[m_tokens.front()];
      m_tokens.pop();

      std::int32_t varSize = std::atoi(std::string(m_tokens.front()).c_str());
      m_tokens.pop();

      std::string varAlias = m_tokens.front();
      m_tokens.pop();

      std::string varName = m_tokens.front();
      m_tokens.pop();

      std::optional<std::pair<std::int32_t, std::int32_t>> bitDepthPair;

      if (varSize != 1 && PinType::parameter != type)
      {
         auto bitDepth = m_tokens.front();
         auto delimPos = bitDepth.find_first_of(':');
         if (delimPos != std::string::npos)
         {
            bitDepthPair = std::make_pair(
                std::atoi(std::string(bitDepth.substr(1, delimPos - 1)).c_str()),
                std::atoi(std::string(bitDepth.substr(delimPos + 1, bitDepth.size() - delimPos - 1)).c_str()));
            m_tokens.pop();
         }
      }
      m_tokens.pop();

      if (type == PinType::parameter)
      {
         descrPtr = std::make_shared<ParamPinDescription>(varAlias, varName);
      }
      else if (varSize != 1)
      {
         descrPtr = std::make_shared<MultiplePinDescription>(type, varAlias, varName, bitDepthPair.value());
      }
      else
      {
         descrPtr = std::make_shared<SimplePinDescription>(type, varAlias, varName);
      }

      return descrPtr;
   }

   void
   Handle::ExtractEndDefinitions()
   {
      m_tokens.pop();
   }

   void
   Handle::ExtractComment()
   {
      auto currentToken = m_tokens.front();
      while (currentToken != "$end")
      {
         m_tokens.pop();
         currentToken = m_tokens.front();
      }
      m_tokens.pop(); // skip end
   }

   void
   Handle::ExtractDumpAll()
   {
      auto currentToken = m_tokens.front();
      const auto vars = ExtractDumpVars();

      for (const auto &[value, alias] : vars)
      {
         m_alias2pin[alias]->SetInitState(value);
      }
      m_tokens.pop(); // skip end
   }

   std::vector<std::pair<std::string, std::string>>
   Handle::ExtractDumpVars()
   {
      std::vector<std::pair<std::string, std::string>> ret;

      auto currentToken = m_tokens.front();
      while (currentToken != "$end")
      {
         m_tokens.pop();

         if (currentToken[0] == 'b')
         {
            auto alias = m_tokens.front();
            m_tokens.pop();
            ret.push_back(std::make_pair(std::string(currentToken.substr(1)), std::string(alias)));
         }
         else
         {
            ret.push_back(std::make_pair(std::string(currentToken.substr(0, 1)), std::string(currentToken.substr(1))));
         }

         currentToken = m_tokens.front();
      }
      return ret;
   }

   void
   Handle::Init(const std::filesystem::path &fileName)
   {
      std::ifstream file(fileName, std::ios::binary);
      if (!file)
      {
         std::cerr << "Can't open " << fileName << '\n';
         return;
      }

      std::ostringstream headerBuf;

      /* читаем поток токенами (разделители – whitespace) */
      std::string tok;
      std::streampos posBeforeTok; // позиция, с которой начался tok
      while (true)
      {
         posBeforeTok = file.tellg(); // запомнили, ГДЕ начинается токен
         if (!(file >> tok))          // EOF/ошибка
            break;

         /* ---- это первый тайм‑штамп? (#<digits>) ---- */
         if (tok.size() > 1 && tok[0] == '#' && std::isdigit(tok[1]) && tok[1] != '0')
         {
            file.seekg(posBeforeTok); // вернули указатель на начало токена
            break;                    // выходим — header закончился
         }

         /* иначе токен – часть header‑а */
         headerBuf << tok << ' ';
      }

      /* позиция начала времянки */
      m_tsOffset = file.tellg();

      /* токенизируем накопленный header */
      m_tokens = Tokenize(headerBuf.str());

      m_filepath = fileName;
      m_size = std::filesystem::file_size(fileName);
      m_fd = ::open(fileName.c_str(), O_RDONLY);
      if (m_fd == -1)
         throw std::runtime_error("open() failed for " + fileName.string());

      m_dataPtr = static_cast<const char *>(
          ::mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, m_fd, 0));
      if (m_dataPtr == MAP_FAILED)
      {
         ::close(m_fd);
         m_fd = -1;
         throw std::runtime_error("mmap() failed for " + fileName.string());
      }
   }

   void
   Handle::FillInitStates(
       const std::vector<std::pair<std::string, std::string>> &dumpVars)
   {
      for (const auto &[value, alias] : dumpVars)
      {
         m_alias2pin[alias]->SetInitState(value);
      }
   }

   void
   Handle::LoadHdr()
   {
      std::string token;
      while (!m_tokens.empty())
      {
         token = m_tokens.front();
         m_tokens.pop();

         if (token == "$date")
         {
            m_date = ExtractDate();
         }
         else if (token == "$version")
         {
            m_version = ExtractVersion();
         }
         else if (token == "$timescale")
         {
            m_timescale = ExtractTimescale();
         }
         else if (token == "$scope")
         {
            m_root = ExtractScope();
         }
         else if (token == "$enddefinitions")
         {
            ExtractEndDefinitions();
         }
         else if (token == "$comment")
         {
            ExtractComment();
         }
         else if (token == "$dumpall")
         {
            ExtractDumpAll();
            m_tokens.pop();
         }
         else if (token == "$dumpvars")
         {
            FillInitStates(ExtractDumpVars());
         }
         /* до тайм‑штампов больше не дойдём */
      }
   }

   std::queue<std::string>
   Handle::Tokenize(std::string_view fileData)
   {
      std::queue<std::string> tokens;
      // Разбиваем содержимое на вектор строк по разделителям "\t", "\n", "\r", " "
      std::size_t beginIdx = 0;
      std::size_t endIdx = 0;
      for (size_t i = 0; i < fileData.length(); ++i)
      {
         // Проверяем, является ли текущий символ одним из разделителей
         if (fileData[i] == '\t' || fileData[i] == '\n' || fileData[i] == '\r' || fileData[i] == ' ')
         {
            if (endIdx - beginIdx)
            {
               auto token = fileData.substr(beginIdx, endIdx - beginIdx);
               tokens.push(std::string(token));
            }
            beginIdx = ++endIdx;
         }
         else
         {
            ++endIdx;
         }
      }
      // Добавляем последний токен, если он не пустой
      if (endIdx - beginIdx)
      {
         tokens.push(std::string(fileData.substr(beginIdx, endIdx - beginIdx)));
      }
      return tokens;
   }

   void
   Handle::LoadSignals()
   {
      using clock = std::chrono::high_resolution_clock;
      auto t0 = clock::now();

      const char *p = m_dataPtr + m_tsOffset;
      const char *end = m_dataPtr + m_size;
      uint64_t curTs = 0;

      auto skip_ws = [&]()
      {
         while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
            ++p;
      };

      std::cout << "---- begin waveform parse ----\n";

      while (p < end)
      {
         skip_ws();
         if (p >= end)
            break;

         /* ---------- timestamp ---------- */
         if (*p == '#')
         {
            ++p;
            uint64_t ts = 0;
            while (p < end && *p >= '0' && *p <= '9')
               ts = ts * 10 + (*p++ - '0');
            curTs = ts;
            // timeStamps.insert(curTs);
            // std::cout << "[#] ts=" << curTs << '\n';
            continue;
         }

         /* ---------- multibit ---------- */
         if (*p == 'b')
         {
            ++p;
            const char *vBeg = p;
            while (p < end && *p != ' ' && *p != '\t' &&
                   *p != '\r' && *p != '\n')
               ++p;
            std::string_view bits(vBeg, p - vBeg);

            skip_ws(); // → alias
            const char *aBeg = p;
            while (p < end && *p != ' ' && *p != '\t' &&
                   *p != '\r' && *p != '\n')
               ++p;
            std::string_view al(aBeg, p - aBeg);

            // std::cout << "[b] ts=" << curTs
            //           << "  alias='" << al
            //           << "'  value=" << bits << '\n';
            std::static_pointer_cast<MultiplePinDescription>(m_alias2pin[al])->m_values.push_back(PinValue{.timestamp = curTs, .value = bits});
            // PinValue pv {bits, pinAlias2DescriptionMap[al]};
            // timeStamp2ValueMap.emplace(curTs, std::move(pv));
            // m_pin2UsedTimestamps[al].emplace(curTs);
            // loadedSignals.insert(al);
            continue;
         }

         /* ---------- $dumpXXX … $end ---------- */
         if (*p == '$')
         {
            const char *lineBeg = p;
            while (p < end && *p != '\n')
               ++p;
            // std::cout.write("[$] skip: ", 9);
            // std::cout.write(lineBeg, p - lineBeg);
            // std::cout.put('\n');
            continue;
         }

         /* ---------- однобитовый ---------- */
         std::string_view val(p++, 1); // '0','1','x','z'
         const char *aBeg = p;
         while (p < end && *p != ' ' && *p != '\t' &&
                *p != '\r' && *p != '\n')
            ++p;
         std::string_view al(aBeg, p - aBeg);

         std::static_pointer_cast<SimplePinDescription>(m_alias2pin[al])->m_values.push_back(PinValue{.timestamp = curTs, .value = val});

         // std::cout << "[1] ts=" << curTs
         //           << "  alias='" << al
         //           << "'  value=" << val << '\n';
      }

      m_maxTimestamp = curTs;
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    clock::now() - t0)
                    .count();
      std::cout << "[ensureSignalLoaded] loaded in " << ms << " ms\n";
   }

   void
   Handle::LoadSignalsParallel()
   {
      using clock = std::chrono::high_resolution_clock;
      auto t0 = clock::now();

      /*------------- 1. выбираем число потоков ------------------*/
      const std::size_t bodySize = m_size - m_tsOffset;
      unsigned hw = std::thread::hardware_concurrency();
      unsigned nThreads =
          (bodySize < 10 * 1024 * 1024) ? 2 : (bodySize < 20 * 1024 * 1024) ? 4
                                                                            : (hw ? hw : 8);

      /*------------- 2. вычисляем границы кусков ----------------*/
      std::vector<const char *> chunkBeg(nThreads + 1);
      chunkBeg[0] = m_dataPtr + m_tsOffset;
      const char *bodyEnd = m_dataPtr + m_size;
      std::size_t chunkSz = bodySize / nThreads;

      for (unsigned i = 1; i < nThreads; ++i)
      {
         const char *p = chunkBeg[0] + i * chunkSz;
         while (p < bodyEnd - 1 && !(p[0] == '\n' && p[1] == '#'))
            ++p;              // двигаемся вперёд до начала строки "#…"
         chunkBeg[i] = p + 1; // ставим точно на '#'
      }
      chunkBeg.back() = bodyEnd; // sentinel

      /*------------- 3. локальные буферы потоков ----------------*/
      struct LocalBuf
      {
         std::unordered_map<std::string_view,
                            std::vector<PinValue>>
             pinData;
         uint64_t maxTs = 0;
      };
      std::vector<LocalBuf> locals(nThreads);

      /*------------- 4. worker-функция --------------------------*/
      auto worker = [&](unsigned idx)
      {
         LocalBuf &L = locals[idx];
         const char *p = chunkBeg[idx];
         const char *e = chunkBeg[idx + 1];
         uint64_t curTs = 0;

         auto skip_ws = [&]()
         {
            while (p < e &&
                   (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
               ++p;
         };

         while (p < e)
         {
            skip_ws();
            if (p >= e)
               break;

            /*--- timestamp -------------------------------------*/
            if (*p == '#')
            {
               ++p;
               uint64_t ts = 0;
               while (p < e && *p >= '0' && *p <= '9')
                  ts = ts * 10 + (*p++ - '0');
               curTs = ts;
               L.maxTs = std::max(L.maxTs, curTs);
               continue;
            }

            /*--- multibit  b<val> <alias> ----------------------*/
            if (*p == 'b')
            {
               ++p;
               const char *vBeg = p;
               while (p < e && *p != ' ' && *p != '\t' &&
                      *p != '\r' && *p != '\n')
                  ++p;
               std::string_view bits(vBeg, p - vBeg);

               skip_ws(); // → alias
               const char *aBeg = p;
               while (p < e && *p != ' ' && *p != '\t' &&
                      *p != '\r' && *p != '\n')
                  ++p;
               std::string_view al(aBeg, p - aBeg);

               L.pinData[al].push_back({curTs, bits});
               continue;
            }

            /*--- пропускаем $dumpXXX … $end --------------------*/
            if (*p == '$')
            {
               while (p < e && *p != '\n')
                  ++p;
               continue;
            }

            /*--- 1-бит  <val><alias> ---------------------------*/
            std::string_view val(p++, 1);
            const char *aBeg = p;
            while (p < e && *p != ' ' && *p != '\t' &&
                   *p != '\r' && *p != '\n')
               ++p;
            std::string_view al(aBeg, p - aBeg);

            L.pinData[al].push_back({curTs, val});
         }
      };

      /*------------- 5. запускаем потоки -----------------------*/
      std::vector<std::thread> workers;
      for (unsigned i = 0; i < nThreads; ++i)
         workers.emplace_back(worker, i);
      for (auto &t : workers)
         t.join();

      /*------------- 6. слияние в основной Handle --------------*/
      m_maxTimestamp = 0;
      for (auto &L : locals)
      {
         m_maxTimestamp = std::max(m_maxTimestamp, L.maxTs);

         for (auto &[alias, vec] : L.pinData)
         {
            auto pinIt = m_alias2pin.find(alias);
            if (pinIt == m_alias2pin.end())
               continue;

            if (pinIt->second->sigType() == SigType::simple)
            {
               auto sp = std::static_pointer_cast<SimplePinDescription>(pinIt->second);
               auto &dst = sp->m_values;
               dst.insert(dst.end(),
                          std::make_move_iterator(vec.begin()),
                          std::make_move_iterator(vec.end()));
            }
            else
            {
               auto mp = std::static_pointer_cast<MultiplePinDescription>(pinIt->second);
               auto &dst = mp->m_values;
               dst.insert(dst.end(),
                          std::make_move_iterator(vec.begin()),
                          std::make_move_iterator(vec.end()));
            }
         }
      }

      /* 7. сортируем каждую ленту изменений (они были отсортированы
            внутри потока, но не между потоками) */
      for (auto &pin : m_pins)
      {
         if (pin->sigType() == SigType::simple)
         {
            auto sp = std::static_pointer_cast<SimplePinDescription>(pin);
            std::sort(sp->m_values.begin(), sp->m_values.end(),
                      [](auto &a, auto &b)
                      { return a.timestamp < b.timestamp; });
         }
         else if (pin->sigType() == SigType::complex)
         {
            auto mp = std::static_pointer_cast<MultiplePinDescription>(pin);
            std::sort(mp->m_values.begin(), mp->m_values.end(),
                      [](auto &a, auto &b)
                      { return a.timestamp < b.timestamp; });
         }
      }

      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    clock::now() - t0)
                    .count();
      std::cout << "[LoadSignalsParallel] "
                << nThreads << " threads, "
                << m_pins.size() << " pins, "
                << "done in " << ms << " ms\n";
   }

   Handle::~Handle()
   {
      ::munmap(const_cast<char *>(m_dataPtr), m_size);
      ::close(m_fd);
   }
} // namespace newVcd