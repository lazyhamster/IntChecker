Macro {
  description="Calculate hash for the file under the cursor";
  area="Shell";
  key="CtrlShiftH";
  action=function()
    far.Show (Plugin.SyncCall("E186306E-3B0D-48C1-9668-ED7CF64C0E65",
              "gethash", "md5", APanel.Path0.."\\"..APanel.Current))
  end;
}