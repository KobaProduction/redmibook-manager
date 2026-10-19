SetShellVarContext all
InitPluginsDir
IfFileExists "$INSTDIR\bin\redmibook-service.exe" 0 rmb_remove_registration
CopyFiles /SILENT "$INSTDIR\bin\redmibook-service.exe" "$PLUGINSDIR\RedmibookManagerMaintenance.exe"
StrCpy $RmbMaintenance "$PLUGINSDIR\rmb-remove"
Exec '"$PLUGINSDIR\RedmibookManagerMaintenance.exe" --maintain "$INSTDIR\bin" "$RmbMaintenance"'
StrCpy $0 0
rmb_remove_wait:
IfFileExists "$RmbMaintenance.ready" rmb_remove_service
IfFileExists "$RmbMaintenance.done" rmb_remove_failed
Sleep 100
IntOp $0 $0 + 1
IntCmp $0 300 rmb_remove_failed rmb_remove_wait rmb_remove_failed
rmb_remove_service:
ExecWait '"$INSTDIR\bin\redmibook-service.exe" --uninstall' $0
IntCmp $0 0 rmb_remove_commit
Goto rmb_remove_failed
rmb_remove_commit:
FileOpen $0 "$RmbMaintenance.remove" w
FileWrite $0 "remove"
FileClose $0
Goto rmb_remove_registration
rmb_remove_failed:
FileOpen $0 "$RmbMaintenance.commit.tmp" w
FileWrite $0 "cancel"
FileClose $0
Rename "$RmbMaintenance.commit.tmp" "$RmbMaintenance.commit"
MessageBox MB_OK|MB_ICONSTOP "Unable to stop or unregister Redmibook Manager components safely." /SD IDOK
SetErrorLevel 2
Abort
rmb_remove_registration:
DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KobaProduction.RedmibookManager.Session"
