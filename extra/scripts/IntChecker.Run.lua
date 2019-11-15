--[[ Integrity Checker by Ariman

Этот сервисный скрипт решает задачи не решаемые плагином - например создание
файлов хэшей для произвольных UNC объектов в данный момент находящихся вне
текущей активной панели Far, например сетевых файлов и каталогов с учётом
структуры дерева каталогов от выбранной точки отсчёта, рекурсивного счёта
хэшей для случаев вида на активной панели выделены файлы и каталоги. Посчитать
для них хэши с учётом вложенных каталогов, остальное игнорировать." при разных
способах указания задачи - либо считать на текушей панели, либо ввести UNC путь
и считать в его пределах, хотя на панели есть выделенные объекты за пределами
UNC пути, диагностика некоторых ошибок, коррекция ошибочного переключения
клавиатуры и ввода неправильных команд, вывод хэшей в формате BSD UNIX, а не
только GNU, запись файлов хэшей по UNC пути в родительский каталог объекта...

Макросы скрипта специально назначены на Alt-H/Alt-G чтобы не перекрывался
функционал хоткея  Ctrl-H - 'Убрать/показать файлы с атрибутом "Скрытый" и
"Системный"' Far-а (см. Справку Far-а: "Клавиатурные команды" - "Команды
управления панелями" раздел "Команды файловой панели") и не конфликтовали с
другими встроенными командами Far-а.

P.S.

GUI диалога нет, и в обозримом будущем его добавление не планируется.

VictorVG @ VikSoft.Ru/

v1.0 - initial version
Wds Jan 15 02:16:30 +0300 2014
v1.1 - refactoring
Mon Jun 15 06:32:20 +0300 2015
v1.1.1 - refactoring
Tue Jun 16 23:25:04 +0300 2015
v1.2 - рефакторинг
Mon Jun 22 05:40:42 +0300 2015
v1.2.1 - рефакторинг
Thu Aug 04 15:09:30 +0300 2016
v1.2.2 - добавлена поддержка SHA3-512
07.11.2017 17:09:21 +0300
v1.3 - рефакторинг и срабатывание макроса на MsLClick по Double Click
17.11.2017 16:12:57 +0300
v1.3.1 - рефакторинг
17.11.2017 20:53:52 +0300
v1.4 - добавлен новый макрос на Alt-G - "Integrity Checker: calc hash for
the file under cursor" умеющий выводить хэш на экран, в буфер обмена ОС или
в хэш файл с именем файла и расширением зависящим от алгоритма в форматах
BSD UNIX или GNU Linux. Макрос написан в виде пошагового мастера с выводом
подсказок в заголовке диалога ввода. Одно символьные хоткеи команд мастера
указаны перед ":" или "-", например 1:CRC32,  G - GNU. Доступные команды в
списке разделены ";". Это сделано для снижения риска ошибок, и справка по
командам стала не нужна.
18.03.2019 02:40:23 +0300
v1.4.1 - Исправим ошибку с пробелами в путях
18.03.2019 10:35:23 +0300
v1.4.2 - уточнение v1.4.1
18.03.2019 13:48:54 +0300
v1.4.3 - при не пустой командной строке если курсор стоит на хэше спросим
оператора что делать? Ok - выполним командную строку, Canсel - проверим хэш.
23.03.2019 22:41:07 +0300
v1.4.4 - мелкий рефакторинг, вопрос к пользователю задаётся по русски,
а чтобы LuaCheck не ругался на длинную строку в mf.msgbox вынесем сообщение
в отдельный оператор local так, чтобы вывелась только одна строка
22.04.2019 17:53:12 +0300
v1.5.0 - рефакторинг, макрос Integrity Checker: calc hash for the file under
cursor переписано с использованием chashex(), реализован расчёт хешей для всех
или выбранных файлов в текущем каталоге;
02.10.2019 08:58:55 +0300
v1.6.0 - расчёт хэшей для указанного пользователем файла или дерева каталогов, рефакторинг;
07.10.2019 22:00:27 +0300
v1.7.0 - считаем хэши для всех файлов в локальном каталоге и его подкаталогах, рефакторинг;
09.10.2019 19:01:24 +0300
v1.8.0 - автовыбора режима счёта "папка/файл" для UNC пути с коррекцией ошибок ввода и
проверкой их существования, хэши сохраняются в обрабатываемый UNC каталог, рефакторинг.
13.10.2019 00:02:14 +0300
v1.8.1 - возвращаемый хэш всегда строка, все сообщения на английском, по умолчанию Target
всегда экран, рефакторинг;
13.10.2019 16:58:01 +0300
v1.9 - вместо chashex() используем chex(), расширенная диагностика и исправление ошибок оператора.
12.11.2019 22:57:42 +0300
--]]

local ICId,ICMID = "E186306E-3B0D-48C1-9668-ED7CF64C0E65","A22F9043-C94A-4037-845C-26ED67E843D1";
local Mask = "/.+\\.(md5|sfv|sha(1|3|256|512)|wrpl)/i";
local MsB,MsF = Mouse.Button,Mouse.EventFlags;
local Msg = "The command line is not empty, but a hash file is under the cursor.\nWhat to do? Command: Ok or Verification: Cancel";
local function chex(hn,pth,ft,r)

--[[
The function for calculating file checksums with support full or relative
UNC paths specified relative to the current panel directory, controlled
recursive processing of directories, partial correction of input errors
for UNC paths and checking the availability of the target object (the user
may not have access rights to it).

User can break calculation if pressed ESC key then returned ErrCode = 4.

Input parameters:

hn  - is hash algorithm name, string
pth - target UNC path, string
ft  - output record format code, false - GNU (default), true - BSD, boolean;
r   - recursion flag true - enable, false - disable (default), boolean;

Returned table included fields by order:

ErrCode    - Error code, integer, value is:
              0 - Success, no errors, HashSumm is valid;
              1 - Empty folder, recursion is disabled or no files found, HashSumm
                  is empty and ObjPath is UNC path;
              2 - Target not exist, HashSumm is invalid and ObjPath is UNC path;
              3 - File Access Denied, FileErrCnt greater 0, HashSumm exclude this files;
              4 - User press break hotkey, FileSucCnt is valid, DirCnt is wrong,
                  HashSumm last string is "Cancel";
HashSumm   - calculated hash sum, string, substring separator is "\n";
ObjPath    - UNC path to object, string;
DirCnt     - scaned directories count, integer;
FileAllCnt - files all count, integer;
FileSucCnt - files success count, integer;
FileErrCnt - files error count, integer;
--]]

   local r0,sum0,s0,f0,d0,p,pnt,dc,ec,err,fc,fa,ft0 = false,"","","","","","",0,0,0,0,0,false;
   r = tostring(r)
   if r:find("true") then r0 = true elseif r:find("false") then r0 = false else r0 = false end;
   if pth:find("\\$") then p = pth:sub(0,-2) elseif pth:find("/$") then
    p = pth:sub(0,-2) else p = pth end;
   ft = tostring(ft)
    if ft:find("true") then ft0 = true elseif ft:find("false") then ft0 = false end;
   if mf.fexist(p) then
    if #mf.fsplit(p,1) == 0 then p = far.ConvertPath(p); pnt = win.GetFileInfo(p).FileName.."\\"; else pnt = "" end
    local dr,da,dn = far.GetDirList(p),{},{};
    if #dr == 0 then
     f0 = win.GetFileInfo(p).FileName
     d0 = p:sub(0, - #f0 - 2);
     fc = 1
     dc = 0
     sum0 = tostring(Plugin.SyncCall(ICId,"gethash",""..hn.."",""..p.."",true));
     if sum0:find("nil") then
      ec = 1
      err = 3
     elseif sum0:find("userabort") then
      ec = 1
      err = 4
      sum0 = "Cancel"
     else
      if ft0 then s0 = hn.." ("..f0..") = "..sum0 else s0 = sum0.." *"..f0; end;
      fa = 1
     end;
    else
     local sem = false;
     d0 = p
     dc = #dr;
     for j = 1,#dr do
      da[j] = dr[j].FileAttributes
      dn[j] = dr[j].FileName
      if not da[j]:find("d") then
       f0 = dn[j]:sub(#p + 2)
       if r0 then sem = true else if not dn[j]:find("\\",#p + 2) then sem = true else sem = false end end;
       if sem then
        fa = fa + 1
         sum0 = tostring(Plugin.SyncCall(ICId,"gethash",""..hn.."",""..dn[j].."",true));
        if sum0 == "userabort" then
         ec = ec + 1
         err = math.max(err,4)
         sum0 = "Cancel"
         break
        elseif sum0 == "nil" then
         ec = ec + 1
         err = math.max(err,3)
        else
         fc = fc + 1
         if ft0 then s0 = s0..hn.." ("..pnt..f0..") = "..sum0.."\n" else s0 = s0..sum0.." *"..pnt..f0.."\n" end;
        end
       end
      end;
     end
      if r0 then dc = dc - fa else dc = 0 end
    end
     if #s0 == 0 and r0 then err = math.max(err,1) end
   else
     err = 2
     s0 = ""
     d0 = p
   end
   local ret = { ErrCode = err, HashSumm = mf.trim(s0), ObjPath = d0, DirCnt = dc, FileAllCnt = fa, FileSucCnt = fc, FileErrCnt = ec };
   return ret;
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
  local arg , ext , fmt , hs , rc , tg = 2 , ".md5" , false , "MD5" , false , "" ;
  arg = tonumber(mf.prompt("1:CRC32;2:MD5;3:SHA1;4:SHA-256;5:SHA-512;6:SHA3-512;7:Whirlpool",nil,1,nil):sub(1,1));
  if arg <= 1 then arg = 1 elseif arg >= 7 then arg = 7 end
  local ti = mf.lcase(mf.prompt("Target: D - display; C - Windows clipboard; F - hash file",nil,1,nil)):sub(1,1);
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
  local mod = mf.lcase(mf.prompt(mmsg,nil,1,nil)):sub(1,2);
  local dm , um , sl = false , false , APanel.Selected;
  if not not mod:find("d") then
   dm = true
  elseif not not mod:find("в") then
   dm = true
  else
   if not not mod:find("u") then
    um = true
   elseif not not mod:find("г") then
    um = true
   end;
  end;
  if not not mod:find("r") then
   rc = true
  elseif not not mod:find("к") then
   rc = true
  end
  local m1=mf.lcase(mf.prompt("Format: B - BSD UNIX; G - GNU",nil,1,nil)):sub(1,1)
  if not not m1:find("b") then
   fmt = true
  elseif not not m1:find("и") then
   fmt = true
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
  local suma,ds,dca,faca,fsca,feca,stat,sg,name,h1 = "","",0,0,0,0,0,false,"";
   if dm then
     name = APanel.Path
    if sl then sg = true end
   elseif um then
    name = mf.prompt("Please, input UNC path and press Enter:",nil,1,nil)
    local ll = far.ConvertPath("..")
    if not not name:sub(1,#ll):find(ll) then
     if APanel.Selected then
      sg = true
     end
    end
   else
   name = APanel.Current
    if sl then
     sg = true
    end
   end;
   if sg then
    for k=1,APanel.ItemCount do
     if Panel.Item(0,k,8) then
      h1 = chex(hs,Panel.Item(0,k,0),fmt,rc)
      suma = suma.."\n"..h1.HashSumm;
      stat = math.max(stat,h1.ErrCode);
      dca = dca + h1.DirCnt
      faca = faca + h1.FileAllCnt
      fsca = fsca + h1.FileSucCnt
      feca = feca + h1.FileErrCnt
      ds = h1.ObjPath
     end
    end
   else
    h1 = chex(hs,name,fmt,rc)
    suma = h1.HashSumm
    stat = h1.ErrCode
    dca = h1.DirCnt
    faca = h1.FileAllCnt
    fsca = h1.FileSucCnt
    feca = h1.FileErrCnt
    ds = h1.ObjPath
   end;
   suma = mf.trim(suma);
   local sem = true
   if stat ~= 0 then
    if stat == 1 then
     sem = false
     far.Message( "Empty folder or no files found.", "IntChecker: error!", "OK", "w" )
    elseif stat == 2 then
     sem = false
     far.Message("UNC patch \n\n"..name.."\n\nnot exist.", "IntChecker: error!", "OK", "lw" )
    else
     if stat == 3 then
      local errMsg = ""
      errMsg = "One or more files is access denied.\n\n".."Dir scaned:    "..dca.."Files found:   "..faca
      errMsg = errMsg.."Files success: "..fsca.."\n".."Files error:   "..feca
      far.Message( errMsg, "IntChecker: warning!", "OK", "lw" )
     elseif stat == 4 then
      sem = false
      far.Message( "User put Cancel command.", "IntChecker: user cancel", "OK" )
     end
    end
   end
   if sem then
    local hsum,cbs = "hashsum";
    if tg == "f" then
     if faca > 1 then
      hsum = ""..ds.."\\".."hashsum"..ext..""
     else
      hsum = ""..ds.."\\"..win.GetFileInfo(name).FileName..ext..""
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
 }