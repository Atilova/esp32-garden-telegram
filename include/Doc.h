const char* helpCommand[] = {
R"rawliteral(
<i><u>КОМАНДЫ СИСТЕМЫ ПОЛИВА</u></i>

<b>12 on</b>
<b>contactor on/off</b>
<b>resanta on/off</b>
<b>24 on/off</b>
<b>deep on/off</b>
<b>garden on/off</b>
<b>deep to ac/resanta</b>
<b>garden to ac/resanta</b>
<b>pool on/off</b> : песочный фильтр бассейна
<b>skimmer on/off</b> : скиммер бассейна
<b>plug  on/off</b> : розетка на газоне
<b>wc on/off</b> : свет в туалете
<b>light on/off</b> : прожектор

<b>.start report=</b><i>ЧЧ:MM</i> : время начала рассылки часовых отчетов
<b>.stop report=</b><i>ЧЧ:MM</i> : время окончания рассылки часовых отчетов
<b>.info hour</b> : часовые отчеты Enable/Disable

<i>page 1</i>
)rawliteral",

R"rawliteral(
<b>.active</b>=<i>1-24</i> : установить кол-во действующих зон полива, max=24
<b>time utro</b>=<i>02:27</i> :  уст. время старта утреннего автополива
<b>time vecher</b>=<i>16:34</i> : уст. время старта вечернего автополива

<b>list</b> : расписание полива на сегодня
<b>list0-6</b> : расписание полива на день (воскр-субб)

<b>zona1-24</b> : параметры автополива для зоны 1-24
<b>zona0</b> : параметры автополива всех зон

<b>.edit</b>=N : редактирование зоны 1-24
    <i>name=</i> : название зоны
    <i>relay=</i> 1-24: номер реле
    <i>enable</i> : полив зоны - разрешен
    <i>blocked</i> : полива зоны - заблокирован
    <i>utro=1-59</i> : длительность полива утром
    <i>vecher=1-59</i> : длительность полива вечером
    <i>sched utro=1111000</i> : недельное утреннее(воскр-субб)
    <i>sched vecher=0000111</i> : недельное вечернее (воскр-субб)
    <i>copy from1-24</i> : скопировать зону (1-24) в текущую
    <i>print</i> : отображает введеные данные  зоны
    <i>quit</i> : выход без сохранения
    <i>exit</i> : сохранить и выйти

  <i>page 2</i>
)rawliteral",

R"rawliteral(
<b>status</b> : текущие параметры системы
<b>info</b> : информация о поливе в мульти- или автополиве

<b>on</b> : режим мы на даче
<b>shutdown</b> :  система выключена, но активна
<b>stdby</b> : дежурный режим наполнения
<b>.stdby</b> : Вкл/Выкл авто выхода в shutdown, после наполнения емкости

<b>options</b> : вывести настойки системы из EEprom
<b>save</b> : сохранить текущие настройки системы в EEprom

<b>tank</b> : запрос уровня воды в емкости
<b>tank reset</b> : обнуление датчиков емкости
<b>stage</b> : текущей FSM системы

<b>apol on</b> : автополив ENABLE
<b>apol off</b> : автополив DISABLE

<b>polN.x</b> : полив зоны N=<i>1-24</i>, x=<i>1-120</i>мин;
x=<i>on/off</i> (вкл/выкл);  x=<i>t</i> -время из расписания

<b>active</b> : поливаемые зоны сейчас
<b>skip</b> : пропустить текущ. зону в автополиве
<b>stop</b> : прервать автополив
<b>pause</b> : приостановить автополив
<b>go</b> : вернуться из паузы в автополив

<b>apol utro</b> :  ручной старт утреннего автополива
<b>apol vecher</b> : ручной старт вечернего автополива

<i>page 3</i>

)rawliteral",

R"rawliteral(
<b>time=</b><i>ЧЧ:MM</i> : установить время
<b>day=</b><i>0:7</i> : задать текущ. день недели 0-7 (воскр-субб)
<b>ntp</b> : синхронизировать время и день недели с ntp сервером
<b>.ntp</b> : вкл/выкл авто синхр при перезагрузке

<b>reboot</b> : перезагрузка ESP32
<b>reboot mega</b> : перезагрузка MEGA
<b>reboot gsm</b> : перезагрузка только GSM модуля SIM800L
<b>reboot dispetcher</b> : перезагрузка Диспетчера Ати-Лева

<b>memory</b> : размер свободного HEAP ESP32
<b>mem</b> :free свободная RAM mega2560
<b>balance</b> : состояние счета SIM карты

<b>/start</b> : загрузка вирт. клавиатуры

<b>/help</b> : команды общие и зоны полива
<b>/helppower</b> : команды силовые цепи и насосы
<b>/help alarm</b> : не поддерживаются, команды охранной системы
<b>/help storm</b> : не поддерживаются, команды модуля грозы

<i>page 4</i>
)rawliteral",

R"rawliteral(
<u><i>КОМАНДЫ УПРАВЛЕНИЯ СИЛОВОЙ ЧАСТЬЮ И НАСОСАМИ</i></u>

<b>ac high=</b><i>221-255</i> : (B), верхний порог, для AC 'норма'
<b>ac low=</b><i>150-220</i> : (B), нижний порог, для AC 'норма'
<b>delay relay=</b><i>1-254</i> : (сек), время ожидания отключения всех реле, после пропадания AC
<b>delay ac=</b><i>0-59</i> : (минуты), задержка возврата насосов c UPS на AC, после восстановления AC 'норма'
<b>garden thr=</b><i>160-220</i> : (B), порог перекл. Garden AC->UPS
<b>deep thr=</b><i>160-220</i> : (B), порог перекл. Deep AC->UPS

<b>.garden</b> : Питание Garden -> AC, UPS, автомат AC&lt;->UPS по заданным значениям
<b>.deep</b> : Питание Deep -> AC, UPS, автомат AC&lt;->UPS по заданным значениям

<b>outrange</b> : вкл/выкл сенсора качества напряжения OUTRANGE AC
<b>.check outrange=</b><i>0-10000</i> : (сек), время проверки стабильности 'норма' outrange

<b>feedback</b> : вкл/выкл общей блокировки контроля датчиков обратной связи
<b>.feedback=</b><i>1:12</i> : вкл/выкл поканальный контроль в цепи обратой связи
<b>feedback error=</b><i>1:254</i> : повторяющихся циклов проверки feedback sensors, до наступления ALERT
<b>feedback list</b> : список feedback каналов

<b>wait ac=</b><i>1-13</i> : (часы), max время ожидания восстановления AC, до аварийного выхода из автополива
<b>time alert=</b><i>1-254</i> : (минуты), max время нахождения в паузе или Alerte
<b>.restore poliv</b> : Вкл/Выкл восстановление реле и зон полива при восстановлении AC220

<b>real ac</b> : реальные значения напряжение сети и Resantы, давление или значения из Telegram
<b>.ac=</b><i>1-255</i> :  значение напряжения сети из Telegram
<b>.res=</b><i>1-255</i> : значение напряжения после Resanta из Telegram
<b>.water=</b><i>0.0-9.9</i> : значение давления воды из Telegram

<i>page 1</i>
)rawliteral",

R"rawliteral(
<b>laser sensor</b> : вкл/выкл лазерного датчика в емкости
<b>tank base=</b><i>1-5000</i> : площадь основания емкости в (м)x1000 (ПИ x кв.радиуса емкости x1000)
<b>tank hight=</b><i>1-3000</i> : высота емкости в мм

<b>deep control</b> : управление насосом скважины по скорости потока, таймеру, контроль выключен
<b>.deep on=</b><i>1-59</i> : (мин), задание времени работы Deep по таймеру
<b>.deep delay=</b><i>1-59</i> : (мин), задание паузы для Deep
<b>.deep low=</b><i>1-500</i> : (л/час), нижний порог скорости потока воды, ниже - отключение насоса Deep
<b>.deep high=</b><i>501-5000</i> : (л/час), верхний скорости потока
<b>.deep wait=</b><i>1-254</i> : (сек), время ожидания заполнения трубы, при отсутствии обр.клапана

<b>garden control</b> : управление работой поливочного насоса датчиком давления, либо внешним реле сухого хода/реле давления
<b>.garden min=</b><i>0.0-9.0</i> : 2,5(атм), нижний порог включение насоса Garden
<b>.garden max=</b><i>0.0-9.9</i> : 4,3(атм), верхний порог отключение насоса Garden
<b>.garden low=</b><i>0.0-9.9</i> : 1,5(атм), давление ниже LOW, вызовет сработку сенсора feedback10

<b>.deep block</b> : блокировка насоса скважины - Deep
<b>.garden block</b> : блокировка поливочного насоса - Garden

<b>.lipo low=</b><i>3.0-4.2</i> : (V), минимальное напряжение LIPO, иначе ошибка
<b>.akb low=</b><i>9.0-15</i> : (V), минимальное напряжение АКБ, иначе ошибка

<i>page 2</i>
)rawliteral"
};

const char* telegramVirtualKeyboard =
  "status \t options \t on \t shutdown \t stdby \t /hide\n"
  "active \t save \t tank \t tank reset \t deep control\n"
  "info \t list \t zona0 \t /help \t /helppower\n"
  "save \t skip \t go \t pause \t stop \t break\n"
  "pool on \t pool off \t skimmer on \t skimmer off\n"
  ".deep block \t .garden block \t apol on \t apol off\n"
  "memory \t balance \t ntp \t /start";

  const char* hideTelegramVirtualKeyboard =
  "/start";