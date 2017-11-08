-- Integrity Checker by Ariman
--
-- Скрипт решает две задачи - вызов главного меню плагина и проверку хэшей
-- с подавлением записи об этом в историю (без мусора оно как спокойнее).
-- Вторая его функция добавлена чтобы вышвырнуть запись из БД ассоциаций
-- ибо там у всех и без того зоопарка хватает. Перед выполнением макроса
-- проверяется расширение файла и коли он не пройдёт проверку, то просто
-- будет молча проигнорирован.
--
-- Ну а с возможностями управления плагином - как всегда, хочется больше,
-- но у плагина GUID-о не хватает. Я бы хотел иметь GUID для для пунктов
-- создания хэшей, сравнения панелей и проверки через буфер обмена чтобы
-- не городить капризный многоэтажный огород с выбором пунктов диалога,
-- но и набор команд управляения меня бы полностью устроил...
--
-- VictorVG @ VikSoft.Ru/
--
-- v1.0 - initial version
-- Wds Jan 15 02:16:30 +0300 2014
-- v1.1 - refactoring
-- Mon Jun 15 06:32:20 +0300 2015
-- v1.1.1 - refactoring
-- Tue Jun 16 23:25:04 +0300 2015
-- v1.2 - рефакторинг
-- Mon Jun 22 05:40:42 +0300 2015
-- v1.2.1 - рефакторинг
-- Thu Aug 04 15:09:30 +0300 2016
-- v1.2.2 - добавлена поддержка SHA3-512
-- 07.11.2017 17:09:21 +0300
--

local ICID="E186306E-3B0D-48C1-9668-ED7CF64C0E65";
local ICMID="A22F9043-C94A-4037-845C-26ED67E843D1";
local Mask="/.+\\.(md5|sfv|sha(1|3|256|512)|wrpl)/i";

Macro{
  id="C7BD288F-E03F-44F1-8E43-DC7BC7CBE4BA";
  area="Shell";
  key="Enter NumEnter MsM1Click MsLClick";
  description="Integrity Checker: check integrity use check summ";
  priority=60;
  flags="EnableOutput";
  condition=function() return mf.fmatch(APanel.Current,Mask)==1; end;
  action=function()
    Far.DisableHistory(-1) Plugin.Command(ICID,APanel.Current);
  end;
}

Macro{
  id="3E69B931-A38E-4119-98E9-6149684B01A1";
  area="Shell";
  key="CtrlH";
  priority=50;
  description="Integrity Checker: show menu";
  action=function()
     Plugin.Menu(ICID,ICMID)
  end;
}