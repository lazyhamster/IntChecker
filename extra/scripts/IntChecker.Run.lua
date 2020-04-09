-- Integrity Checker by Ariman

-- Этот сервисный скрипт решает задачи не решаемые плагином - например создание
-- файлов хэшей для произвольных UNC объектов в данный момент находящихся вне
-- текущей активной панели Far, рекурсивного счёта хэшей для случаев вида "на
-- активной панели выделены файлы и каталоги. Посчитать для них хэши с учётом
-- вложенных каталогов, остальное игнорировать." при разных способах постановки
-- задачи - либо считать на текушей панели, либо ввести UNC путь и считать в его
-- пределах, хотя на панели есть выделенные объекты за пределами UNC пути,
-- диагностики некоторых ошибок, в.ч. ошибочного переключения клавиатуры с
-- вводом неправильных команд, вывод хэшей в формате BSD UNIX, а не только GNU,
-- запись файлов хэшей по UNC пути в родительский каталог объекта...

-- Макросы скрипта специально назначены на Alt-H/Alt-G чтобы не перекрывался
-- функционал хоткея  Ctrl-H - 'Убрать/показать файлы с атрибутом "Скрытый" и
-- "Системный"' Far-а (см. Справку Far-а: "Клавиатурные команды" - "Команды
-- управления панелями" раздел "Команды файловой панели") и не конфликтовали с
-- другими встроенными командами Far-а.

-- P.S.

-- GUI диалога нет, и в обозримом будущем его добавление не планируется.

-- VictorVG @ VikSoft.Ru/

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
-- v1.4.3 - при не пустой командной строке если курсор стоит на хэше спросим
-- оператора что делать? Ok - выполним командную строку, Canсel - проверим хэш.
-- 23.03.2019 22:41:07 +0300
-- v1.4.4 - мелкий рефакторинг, вопрос к пользователю задаётся по русски,
-- а чтобы LuaCheck не ругался на длинную строку в mf.msgbox вынесем сообщение
-- в отдельный оператор local так, чтобы вывелась только одна строка
-- 22.04.2019 17:53:12 +0300
-- v1.5.0 - рефакторинг, макрос Integrity Checker: calc hash for the file under
-- cursor переписано с использованием chashex(), реализован расчёт хешей для всех
-- или выбранных файлов в текущем каталоге;
-- 02.10.2019 08:58:55 +0300
-- v1.6.0 - расчёт хэшей для указанного пользователем файла или дерева каталогов, рефакторинг;
-- 07.10.2019 22:00:27 +0300
-- v1.7.0 - считаем хэши для всех файлов в локальном каталоге и его подкаталогах, рефакторинг;
-- 09.10.2019 19:01:24 +0300
-- v1.8.0 - автовыбора режима счёта "папка/файл" для UNC пути с коррекцией ошибок ввода и
-- проверкой их существования, хэши сохраняются в обрабатываемый UNC каталог, рефакторинг.
-- 13.10.2019 00:02:14 +0300
-- v1.8.1 - возвращаемый хэш всегда строка, все сообщения на английском, по умолчанию Target
-- всегда экран, рефакторинг;
-- 13.10.2019 16:58:01 +0300
-- v1.9 - вместо chashex() используем chex(), расширенная диагностика и исправление ошибок оператора.
-- 12.11.2019 22:57:42 +0300
-- v1.9.1 - "Integrity Checker: calculate hash for ..." обработка нажатия ESC, уточнение v1.9
-- 27.01.2020 02:59:44 +0300
-- v2.0 - chex() v2.1, пути в стиле Windows/UNIX, новая диагностика ошибок, требует IntChecker v2.80, рефакторинг
-- 09.03.2020 06:50:43 +0300
--
local ICId,ICMID = "E186306E-3B0D-48C1-9668-ED7CF64C0E65","A22F9043-C94A-4037-845C-26ED67E843D1";
local Mask = "/.+\\.(md5|sfv|sha(1|3|256|512)|wrpl)/i";
local MsB,MsF = Mouse.Button,Mouse.EventFlags;
local Msg = "The command line is not empty, but a hash file is under the cursor.\nWhat to do? Command: Ok or Verification: Cancel";
local function chex (pth,hn,fh,ft,r,sld)
local ds,ec,fc,fl,pt,rn,rt,s0 = "",0,0,4,"",0,"","";
pth = tostring(pth)
if tostring(r):find("true") then r = true else r = false end;
if tostring(ft):find("true") then ft = true else ft = false end;
 if #mf.fsplit(pth,1) == 0 then
  if pth:find("\\",1,1) then pth = pth:sub(2) elseif pth:find("/",1,1) then pth = pth:sub(2) end
  pt = APanel.Path.."\\"..pth
 else
  pt = pth
 end
 pt = mf.fsplit(pt,1)..mf.fsplit(pt,2)..mf.fsplit(pt,4)..mf.fsplit(pt,8)
 if mf.fexist(pth) then
  local f = mf.testfolder(pth)
  if f == 2 then
    local df = ""
    if r then fl = 6 else fl = 4 end;
     pt = pth
     if sld then df = win.GetFileInfo(pt).FileName.."\\" else df = "" end
     far.RecursiveSearch(""..pt.."","*>>D",function (itm,fp,hn,ft)
     rt = tostring(Plugin.SyncCall(ICId,"gethash",""..hn.."",""..fp.."",true));
     if rt == "userabort" then
      rn = 6
      return rn;
     else
      if rt ~= "false" and #rt ~= 0 then
       fc = fc + 1
       if fp:find("\\\\") then fp = fp:sub(#pt + 1) else fp = fp:sub(#pt + 2) end
       fp = df..fp
       if tostring(fh):find("true") then fp = fp:gsub("\\","/") end
       if ft then s0 = s0..hn.." ("..fp..") = "..rt.."\n" else s0 = s0..rt.." *"..fp.."\n" end
      else
       if f == -1 then
        rn = math.max(rn,4)
        ec = ec + 1
       elseif #rt == 0 then
        rn = math.max(rn,3)
        ec = ec + 1
       end
      end
     end
    end,fl,hn,ft)
    ds = pth
  else
   local obj = win.GetFileInfo(pt)
   if not not obj then
    rt = tostring(Plugin.SyncCall(ICId,"gethash",""..hn.."",""..pt.."",true));
    if rt == "userabort" then
     rn = 6
     ec = 1
    else
     if rt ~= "false" and #rt ~= 0 then
      fc = 1
      if ft then s0 = hn.." ("..obj.FileName..") = "..rt else s0 = rt.." *"..obj.FileName end;
      ds = pt:sub(1,#pt - #obj.FileName - 1)
     elseif #rt == 0 then
      rn = 3
      ec = 1
     end
    end
   end
  end
 end
 local ret = {RetCode = rn, FileCnt = fc, ErrCnt = ec, HashSumm = mf.trim(s0), DirSW = ds }
 return ret
end;

Macro{
  id = "C7BD288F-E03F-44F1-8E43-DC7BC7CBE4BA";
  area = "Shell";
  key = "Enter NumEnter MsM1Click";
  description = "Integrity Checker: check integrity use check summ";
  priority = 60;
  flags = "EnableOutput";
  condition = function() return (mf.fmatch(APanel.Current,Mask)==1 and not (MsB==0x0001 and MsF==0x0001)) end;
  action = function()
  if not CmdLine.Empty then
   if mf.msgbox("Question to the user:",Msg,0x00020000) == 1 then
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
  id = "3E69B931-A38E-4119-98E9-6149684B01A1";
  area = "Shell";
  key = "AltH";
  priority = 50;
  description = "Integrity Checker: show menu";
  action = function()
    Far.DisableHistory(-1) Plugin.Menu(ICId,ICMID)
  end;
}

Macro{
  id = "A3C223FD-6769-4A6E-B7E6-4CC59DE7B6A2";
  area = "Shell";
  key = "AltG";
  description = "Integrity Checker: calculate hash for ...";
  priority = 50;
  condition = function() return not APanel.Plugin end;
  action = function()
   local arg , ext , fmt , hs , rc , tg, ofm = 2 , ".md5" , false , "MD5" , false , "" , false;
   arg = mf.prompt("1:CRC32;2:MD5;3:SHA1;4:SHA-256;5:SHA-512;6:SHA3-512;7:Whirlpool",nil,1,nil);
   if arg ~= false then
    arg = tonumber(arg:sub(1,1))
    if arg <= 1 then arg = 1 elseif arg >= 7 then arg = 7 end
    local ti = mf.prompt("Target: D - display; C - Windows clipboard; F - hash file",nil,1,nil);
    if ti ~= false then
     ti = mf.lcase(ti:sub(1,1));
     if ti:find("f") then
      tg = "f"
     elseif ti:find("c") then
      tg = "c"
     else
      if ti:find("а") then
       tg = "f"
      elseif ti:find("с") then
       tg = "c"
      else
       tg = "d"
      end
     end
     local mmsg = "F - Und. cursor;D[R] - Curr. dir;U[R] - UNC path;[R] - recurs."
     local m1 , dm , um = mf.lcase(mf.prompt(mmsg,nil,1,nil)):sub(1,2), false , false;
     if m1 ~= false then
      if not not m1:find("d") then
       dm = true
      elseif not not m1:find("в") then
       dm = true
      else
       if not not m1:find("u") then
        um = true
       elseif not not m1:find("г") then
        um = true
       end;
      end;
      if not not m1:find("r") then
       rc = true
      elseif not not m1:find("к") then
       rc = true
      end
      local m2 = mf.lcase(mf.prompt("Format: B - BSD UNIX; G - GNU",nil,1,nil)):sub(1,1)
      if not not m2:find("b") then
       fmt = true
      elseif not not m2:find("и") then
       fmt = true
      end;
      local m3 = mf.lcase(mf.prompt("Out string path type: W - Windows; U - UNIX",nil,1,nil)):sub(1,1)
      if not not m3:find("u") then
       ofm = true
      elseif not not m3:find("г") then
       ofm = true
      end;
      if arg == 1 then
       hs = "CRC32"
       ext = ".sfv"
      elseif arg == 3 then
       hs = "SHA1"
       ext = ".sha1"
      else
       if arg == 4 then
        hs = "SHA-256"
        ext = ".sha256"
       elseif arg == 5 then
        hs = "SHA-512"
        ext=".sha512"
       else
        if arg == 6 then
         hs = "SHA3-512"
         ext = ".sha3"
        elseif arg == 7 then
         hs = "Whirlpool"
         ext = ".wrpl"
        end
       end;
      end;
      local eca,dsv,fca,name,stat,suma,sgn = 0,"",0,"", 0,"",false
      if dm then
        name = APanel.Path
       if APanel.Selected then sgn = true else sgn = false end
      elseif um then
       name = mf.prompt("Please, input UNC path and press Enter:",nil,1,nil)
       if name ~= false then
        local ll = far.ConvertPath("..")
        if not not name:sub(1,#ll):find(ll) then
         if APanel.Selected then sgn = true else sgn = false end
        end
       else
        if far.Message("Pressed ESC button. Save result to current dir?","Question:",";OkCancel","w") == 1 then
         name = APanel.Current
        else
         tg = "c"
         far.Message("Result mast copy to clipboard","Information:","OK")
        end;
       end;
      else
       name = APanel.Current
       if APanel.Selected then sgn = true else sgn = false end
      end;
      if sgn then
       for k=1,APanel.ItemCount do
        if Panel.Item(0,k,8) then
         local h1 = chex(Panel.Item(0,k,0),hs,ofm,fmt,rc,true)
         suma = suma.."\n"..h1.HashSumm;
         stat = math.max(stat,h1.RetCode);
         fca = fca + h1.FileCnt
         eca = eca + h1.ErrCnt
         dsv = h1.DirSW
        end
       end
      else
       local h1 = chex(name,hs,ofm,fmt,rc,false)
       suma = h1.HashSumm
       stat = h1.RetCode
       fca = h1.FileCnt
       eca = h1.ErrCnt
       dsv = h1.DirSW
      end;
      suma = mf.trim(suma);
      local sem = true
      if stat ~= 0 then
       if stat == 1 then
        sem = false
        far.Message("UNC patch \n\n"..name.."\n\nnot exist.", "IntChecker: error!", "OK", "lw")
       end
       if stat == 2 then
        sem = false
        far.Message("Recursion is disabled then UNC patch\n\n"..name.."\n\nnot found.","IntChecker: error!","OK","lw")
       end
       if stat == 3 then
        far.Message("For some files access denied, skiped.", "IntChecker: warning!", "OK")
       elseif stat == 4 then
        far.Message("For some directory access denied, skiped.", "IntChecker: warning!", "OK")
       end
       if stat == 5 then
        far.Message("Some dir is empty, skiped.", "IntChecker: warning!", "OK")
       end
       if stat == 6 then
        sem = false
        far.Message( "User put Cancel command.", "IntChecker: user cancel", "OK", "lw" )
       end
      end
      if sem then
       local hsum,cbs = "hashsum";
       if tg == "f" then
        if fca > 1 then
         hsum = ""..dsv.."\\".."hashsum"..ext..""
        else
         hsum = ""..dsv.."\\"..win.GetFileInfo(name).FileName..ext..""
        end;
        if Panel.FExist(hsum,0) >= 1 then
         win.DeleteFile(hsum)
        end;
        local sum = io.open(hsum,"wb");
        if sum ~= nil then
         sum:write(suma);
         sum:flush();
         sum:close();
        else
         local dMsg="Can\'t write hash to file. Do your like copy it to clipboard?\nAlso hash is be show on screen.";
         if far.Message(dMsg,"Can\'t write hash file",";OkCancel","w",nil,nil) == 1 then
          cbs=mf.clip(5,1);
          mf.clip(suma);
          if cbs == 2 then
           mf.clip(5,2)
          end;
         end;
          far.Show(suma);
        end;
       elseif tg == "c" then
        cbs = mf.clip(5,1);
        mf.clip(1,suma);
        if cbs == 2 then
         mf.clip(5,2)
        end;
       else
        far.Show(suma)
       end;
      end;
     end;
    end;
   end;
  end;
 }