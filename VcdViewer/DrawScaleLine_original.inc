void WaveformView::DrawScaleLine(bool reset)
{
   if (!m_handle)
   {
      return;
   }

   double visibleStart = 0;
   double visibleEnd = 0;
   double visibleWidth = 0;
   // Получаем видимую область
   if (reset)
   {
      visibleStart = 0;
      visibleEnd = m_handle->maxTs();
      if (visibleEnd == 0)
      {
         return;
      }
      visibleWidth = visibleEnd;
   }
   else
   {
      QRectF visibleRect = mapToScene(viewport()->rect()).boundingRect();
      visibleStart = visibleRect.left();
      visibleEnd = visibleRect.right() <= m_handle->maxTs() ? visibleRect.right() : m_handle->maxTs();
      visibleWidth = visibleEnd - visibleStart;
   }

   m_scaleScene->clear();
   m_scaleScene->setSceneRect(0, -SCALE_LINE_HEIGHT, m_handle->maxTs(), SCALE_LINE_HEIGHT);

   // Определяем шаги для различных единиц измерения
   static const std::unordered_map<std::string, double> scaleMap = {
       {"ps", 1.0}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}};
   static const std::vector<std::string> units = {"ps", "ns", "us", "ms", "s"};

   auto iterFirstNotNumber = std::find_if(m_handle->timeScale().begin(), m_handle->timeScale().end(), [](char c)
                                          { return !std::isdigit(static_cast<unsigned char>(c)); });

   std::size_t mulFactor = std::stol(std::string(m_handle->timeScale().substr(0, std::distance(m_handle->timeScale().begin(), iterFirstNotNumber))));
   std::string currentUnit = std::string(m_handle->timeScale().substr(std::distance(m_handle->timeScale().begin(), iterFirstNotNumber)));
   double timescaleFactor = scaleMap.at(currentUnit);
   int unitIndex = std::distance(units.begin(), std::find(units.begin(), units.end(), currentUnit));

   // Начальный расчет шага деления для оптимальной видимости
   double optimalStep = visibleWidth / 10.0; // Стремимся к 10 основным делениям в видимой области
   double increment = optimalStep * timescaleFactor;

   // Переход на более крупную единицу, если шаг слишком мал
   while (unitIndex < units.size() - 1 && increment >= 1000)
   {
      increment /= 1000;
      timescaleFactor /= 1000;
      ++unitIndex;
   }
   currentUnit = units[unitIndex];

   // Приводим increment к «круглому» числу для крупных делений (например, 10, 50, 100)
   double base = std::pow(10, std::floor(std::log10(increment)));
   if (increment / base >= 5)
      increment = 5 * base;
   else if (increment / base >= 2)
      increment = 2 * base;
   else
      increment = base;

   double labelStep = increment / timescaleFactor;

   // Проверяем, достаточно ли места для текста, увеличивая шаг при необходимости
   double minTextSpacing = 50.0; // Минимальное расстояние между метками (в пикселях)
   QRectF lastLabelRect;         // Последняя нарисованная текстовая метка

   // Определяем границы рисования, используя видимую область с небольшим запасом
   double minCoord = std::max(0.0, std::floor(visibleStart / labelStep) * labelStep - labelStep);
   double maxCoord = std::ceil(visibleEnd / labelStep) * labelStep + labelStep;
   if (maxCoord >= m_handle->maxTs())
   {
      maxCoord = m_handle->maxTs();
   }

   // линия с временными метками
   m_scaleScene->addLine(minCoord, -SCALE_LINE_HEIGHT / 2, maxCoord, -SCALE_LINE_HEIGHT / 2, QPen(Qt::blue, 1)); // Основная линия

   QGraphicsTextItem *label = m_scaleScene->addText("00");
   const auto minDistance = label->boundingRect().width();
   delete label;

   for (auto *it : m_lineItems)
   {
      if (it)
      {
         m_scene->removeItem(it);
      }
   }
   m_lineItems.clear();

   // Рисуем основные деления с метками только в видимой области
   for (double xCoord = minCoord; xCoord <= maxCoord; xCoord += labelStep)
   {
      QPen pen(QColor(0, 0, 255, 128), 1); // Полупрозрачный синий цвет
      pen.setCosmetic(true);

      // Добавляем метку
      uint64_t timeValue = static_cast<uint64_t>(xCoord * timescaleFactor * mulFactor);
      int adjustedUnitIndex = unitIndex; // Начинаем с текущего индекса единицы
      while (adjustedUnitIndex < units.size() - 1 && timeValue >= 1000)
      {
         timeValue /= 1000;
         ++adjustedUnitIndex; // Переходим на следующий индекс
      }
      std::string adjustedUnit = units[adjustedUnitIndex];

      QString labelText = QString::number(timeValue) + QString::fromStdString(adjustedUnit);
      if (labelText.left(1) == "0")
      {
         continue;
      }

      QGraphicsTextItem *label = m_scaleScene->addText(labelText);

      QFont font = label->font();
      font.setPointSize(8);
      label->setFont(font);
      label->setFlag(QGraphicsItem::ItemIgnoresTransformations); // Чтобы текст не масштабировался
      label->setDefaultTextColor(Qt::white);

      // Располагаем текст по центру линии
      QRectF textRect = label->boundingRect();
      double labelX = xCoord - (textRect.width() / 2) / transform().m11();

      // Проверяем, накладывается ли текущая метка на предыдущую
      if (!lastLabelRect.isEmpty() && lastLabelRect.intersects(QRectF(labelX - minDistance, -SCALE_LINE_HEIGHT - 5, textRect.width() + minDistance, textRect.height())))
      {
         // Если накладывается, пропускаем эту метку и линию
         delete label;

         auto pen = QPen(Qt::blue, 1, Qt::DashLine);
         pen.setCosmetic(true);
         m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Промежуточная линия
         continue;
      }

      label->setPos(labelX, -SCALE_LINE_HEIGHT - 5); // Центрируем текст по оси X
      lastLabelRect = QRectF(labelX, -SCALE_LINE_HEIGHT - 5, textRect.width(), textRect.height());

      // Рисуем линию только если текстовая метка была добавлена
      m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Основная линия

      std::size_t height = m_signals.size() * (WAVEFORM_HEIGHT + SPACING);
      auto *sceneLine = m_scene->addLine(xCoord, 0, xCoord, sceneRect().height(), pen); // Основная линия
      m_lineItems.push_back(sceneLine);
      sceneLine->setZValue(Layers_ScaleLine);
   }

   // Рисуем промежуточные деления только в видимой области
   double subStep = labelStep / 5.0;
   for (double xCoord = minCoord; xCoord <= maxCoord; xCoord += subStep)
   {
      if (std::fmod(xCoord, labelStep) != 0) // Пропускаем основные деления
      {
         auto pen = QPen(Qt::blue, 1, Qt::DashLine);
         pen.setCosmetic(true);
         auto line = m_scaleScene->addLine(xCoord, -SCALE_LINE_HEIGHT / 2, xCoord, SCALE_LINE_HEIGHT, pen); // Промежуточная линия
         // m_lineItems.push_back(line);
         line->setZValue(Layers_ScaleLine);
      }
   }
}