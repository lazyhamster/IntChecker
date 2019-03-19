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
-- но и набор команд управления меня бы полностью устроил...
--
-- Макрос специально назначен на Alt-H чтобы не перекрывал функционал хоткея
-- Ctrl-H - 'Убрать/показать файлы с атрибутом "Скрытый" и "Системный"' Far-а
-- (см. Справку Far-а: "Клавиатурные команды" - "Команды управления панелями"
-- раздел "Команды файловой панели").
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
-- v1.3 - рефакторинг и срабатывание макроса на MsLClick по Double Click
-- 17.11.2017 16:12:57 +0300
-- v1.3.1 - рефакторинг
-- 17.11.2017 20:53:52 +0300
-- v1.4 - добавлен новый макрос на Alt-G - "Integrity Checker: calc hash for
-- the file under cursor" умеющий выводить хэш на экран, в буфер обмена ОС или
-- в хэш файл с именем файла и расширением зависящим от алгоритма в форматах
-- BSD UNIX или GNU Linux. Макрос написан в виде пошагового мастера с выводом
-- подсказок в заголовке диалога ввода. Одно символьные хоткеи команд мастера
-- указаны перед ":" или "-", например 1:CRC32,  G - GNU. Доступные команды в
-- списке разделены ";". Это сделано для снижения риска ошибок, и справка по
-- командам стала не нужна.
-- 18.03.2019 02:40:23 +0300
-- v1.4.1 - Исправим ошибку с пробелами в путях
-- 18.03.2019 10:35:23 +0300
-- v1.4.2 - уточнение v1.4.1
-- 18.03.2019 13:48:54 +0300

local ICID="E186306E-3B0D-48C1-9668-ED7CF64C0E65";
local ICMID="A22F9043-C94A-4037-845C-26ED67E843D1";
local Mask="/.+\\.(md5|sfv|sha(1|3|256|512)|wrpl)/i";
local arg,algo,cmd,ext,hash,mod,pt,prn,tp;
local MsB=Mouse.Button;
local MsF=Mouse.EventFlags;

Macro{
  id="C7BD288F-E03F-44F1-8E43-DC7BC7CBE4BA";
  area="Shell";
  key="Enter NumEnter MsM1Click";
  description="Integrity Checker: check integrity use check summ";
  priority=60;
  flags="EnableOutput";
  condition=function() return (mf.fmatch(APanel.Current,Mask)==1 and not (MsB==0x0001 and MsF==0x0001)) end;
  action=function()
    Far.DisableHistory(-1) Plugin.Command(ICID,APanel.Current);
  end;
}

Macro{
  id="3E69B931-A38E-4119-98E9-6149684B01A1";
  area="Shell";
  key="AltH";
  priority=50;
  description="Integrity Checker: show menu";
  action=function()
    Far.DisableHistory(-1) Plugin.Menu(ICID,ICMID)
  end;
}

Macro{
  id="A3C223FD-6769-4A6E-B7E6-4CC59DE7B6A2";
  area="Shell";
  key="AltG";
  description="Integrity Checker: calc hash for the file under cursor";
  priority=50;
  condition=function() return not APanel.Folder end;
  action=function()
  arg = mf.prompt("1:CRC32;2:MD5;3:SHA1;4:SHA-256;5:SHA-512;6:SHA3-512;7:Whirlpool",nil,1,nil);
  mod = mf.lcase(mf.prompt("Target: D - display; C - Windows ClipBoard; F - hash file",nil,1,nil));
tp = mf.lcase( mf.prompt("Hash file format: B - BSD UNIX; G - GNU",nil,1,nil));
  if arg == "1" then algo="CRC32"; ext="sfv";
     elseif arg == "2" then algo="MD5"; ext="md5"; else if arg == "3" then algo="SHA1"; ext="sha1";
       elseif arg == "4" then algo="SHA-256"; ext="sha256"; else if arg == "5" then algo="SHA-512"; ext="sha512";
        elseif arg == "6" then algo="SHA3-512"; ext="sha3";else if arg == "7" then algo="Whirlpool"; ext="wrpl";
        else algo="SHA-256"; ext="sha256";
        end;
       end;
     end;
   end;
   hash = Plugin.SyncCall(ICID,"gethash", algo, APanel.Path0.."\\"..APanel.Current);
   if mod=="d" then far.Show(hash);
    elseif mod=="c" then mf.clip(5,1);
               if tp=="b" then prn=algo.." ("..APanel.Current..") = "..hash;
             else prn=hash.." *"..APanel.Current;
         end;
     mf.clip(1,prn);
     else
     cmd=CmdLine.Value;
     pt="\""..APanel.Path0.."\\"..mf.fsplit(APanel.Current,4).."."..ext.."\"";
     Far.DisableHistory(-1)
         if tp=="b" then prn="@echo".." "..algo.." ("..APanel.Current..") = "..hash.." > "..pt;
             else prn="@echo".." "..hash.." *"..APanel.Current.." > "..pt;
         end;
         mf.print(prn) Keys("Enter");
      end;
     mf.print(cmd);
  end;
}