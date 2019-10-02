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
-- v1.4.3 - при непустой командной строке если курсор стоит на хэше спросим
-- оператора что делать? Ok - выполним командную строку, Canсel - проверим хэш.
-- 23.03.2019 22:41:07 +0300
-- v1.4.4 - мелкий рефакторинг, вопрос к пользователю задаётся по русски,
-- а чтобы LuaCheck не ругался на длинную строку в mf.msgbox вынесем сообщение
-- в отдельный оператор local так, чтобы вывелась только одна строка
-- 22.04.2019 17:53:12 +0300
-- v1.5.0 - рефакторинг, макрос Integrity Checker: calc hash for the file under
-- cursor переписан с использованием chashex(), реализован расчёт хешей для всех
-- или выбранных файлов в теущем каталоге;
-- 02.10.2019 08:58:55 +0300

local ICId,ICMID="E186306E-3B0D-48C1-9668-ED7CF64C0E65","A22F9043-C94A-4037-845C-26ED67E843D1";
local Mask="/.+\\.(md5|sfv|sha(1|3|256|512)|wrpl)/i";
local MsB,MsF=Mouse.Button,Mouse.EventFlags;
local Msg="Командая строка не пуста, но под курсором хэш файл. Что выполнить? Команду: Ok или Проверку: Cancel";
local function chashex(hs,ft,md)
local t,s,sv,i,j=mf.string(""),mf.string(""),APanel.CurPos,0,0;
-- ft - формат 1 == BSD, 0 == GNU, по умолчанию GNU;
-- hs - имя алгоритма;
-- md - режим счёта 0 - под курсором, 1 - выбранные или весь каталог;
local function ch(hn,pth)
 if APanel.Plugin then do return -1 end
   else do return mf.string(Plugin.SyncCall(ICId,"gethash",""..hn.."",""..pth.."")) end end;
end
 if md == 1  then
  if APanel.Selected then
  j=APanel.SelCount;
   for i=1,j,1 do Panel.SetPosIdx(0,i,1);
    if not APanel.Folder then
     if ft == 1 then s=hs.." ("..APanel.Current..") = "..ch(hs,APanel.Current)
       else s=ch(hs,APanel.Current).." *"..APanel.Current end;
     if i < j then t=t..s.."\n" else t=t..s end;
    end
   end
   Panel.SetPosIdx(0,sv);
   else
   Panel.SetPosIdx(0,1);
   i=APanel.ItemCount
   if APanel.Root then i=i-1; j=2 else j=1 end;
   for i=j,i,1 do Panel.SetPosIdx(0,i);
    if not APanel.Folder then
     if ft == 1 then s=hs.." ("..APanel.Current..") = "..ch(hs,APanel.Current)
       else s=ch(hs,APanel.Current).." *"..APanel.Current end;
     if i < APanel.ItemCount then t=t..s.."\n" else t=t..s end;
    end;
   end;
   Panel.SetPosIdx(0,sv);
  end;
  else
   if not APanel.Root then if not APanel.Folder then
    if ft == 1 then t=hs.." ("..APanel.Current..") = "..ch(hs,APanel.Current)
        else t=ch(hs,APanel.Current).." *"..APanel.Current end;
   end;
   end;
  end;
  return mf.trim(t);
end;

Macro{
  id="C7BD288F-E03F-44F1-8E43-DC7BC7CBE4BA";
  area="Shell";
  key="Enter NumEnter MsM1Click";
  description="Integrity Checker: check integrity use check summ";
  priority=60;
  flags="EnableOutput";
  condition=function() return (mf.fmatch(APanel.Current,Mask)==1 and not (MsB==0x0001 and MsF==0x0001)) end;
  action=function()
  if not CmdLine.Empty then
 if mf.msgbox("Вопрос к пользователю:",Msg,0x00020000) == 1
        then
          Keys("Enter");
        else
          Far.DisableHistory(-1) Plugin.Command(ICId,APanel.Current);
        end;
    else
      Far.DisableHistory(-1) Plugin.Command(ICId,APanel.Current);
   end;
  end;
}

Macro{
  id="3E69B931-A38E-4119-98E9-6149684B01A1";
  area="Shell";
  key="AltH";
  priority=50;
  description="Integrity Checker: show menu";
  action=function()
    Far.DisableHistory(-1) Plugin.Menu(ICId,ICMID)
  end;
}

Macro{
  id="A3C223FD-6769-4A6E-B7E6-4CC59DE7B6A2";
  area="Shell";
  key="AltG";
  description="Integrity Checker: calc hash for the file under cursor";
  priority=50;
  condition=function() return not APanel.Plugin end;
  action=function()
  local mod,tg,sem,hsum,hash,arg,hs,ext,md,ft="c","f",0,"hashlist","",2,"MD5","md5",0,0;
  arg = mf.lcase(mf.prompt("1:CRC32;2:MD5;3:SHA1;4:SHA-256;5:SHA-512;6:SHA3-512;7:Whirlpool",nil,1,nil));
  tg = mf.lcase(mf.prompt("Target: D - display; C - Windows clipboard; F - hash file",nil,1,nil))
  mod = mf.lcase(mf.prompt("Calc for: A - all files; C - under cursor; S - selected files",nil,1,nil));
  if mf.lcase(mf.prompt("Hash file format: B - BSD UNIX; G - GNU",nil,1,nil)) == "b" then ft=1; else ft=0; end;
   if arg == "1" then hs="CRC32"; ext=".sfv";
     elseif arg == "2" then hs="MD5"; ext=".md5";
      else
       if arg == "3" then hs="SHA1"; ext=".sha1";
         elseif arg == "4" then hs="SHA-256"; ext=".sha256";
          else
           if arg == "5" then hs="SHA-512"; ext=".sha512";
            elseif arg == "6" then hs="SHA3-512"; ext=".sha3";
             else
              if arg == "7" then hs="Whirlpool"; ext=".wrpl";
               else
                hs="SHA-256"; ext=".sha256";
               end;
              end;
           end;
        end;
       if (mod == "a" or mod == "s") then md=1; else md=0; end;
     if  tg=="d" then far.Show(chashex(hs,ft,md))
       elseif tg == "c" then
        sem=mf.clip(5,1);
          mf.clip(1,chashex(hs,ft,md));
          if sem==2 then mf.clip(5,2) end;
       else
        if md == 0 then hsum=mf.fsplit(APanel.Current,4)..ext; else hsum=hsum..ext end;
          if Panel.FExist(hsum,0) >=1 then Keys("ShiftF8 Enter") end;
           hash=chashex(hs,ft,md);
           local file = io.open(hsum,"wb")
          file:write(hash);
        file:flush();
      file:close();
    end;
end;
}