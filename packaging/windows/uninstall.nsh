SetShellVarContext all
InitPluginsDir
IfFileExists "$INSTDIR\bin\redmibook-service.exe" 0 rmb_remove_registration
StrCpy $RmbMaintenance "$PLUGINSDIR\rmb-remove"
; Optional registry reads in the NSIS template can leave the error flag set.
; Check each operation's own result instead of inheriting that flag.
ClearErrors
CopyFiles /SILENT "$INSTDIR\bin\redmibook-service.exe" "$PLUGINSDIR\RedmibookManagerMaintenance.exe"
IfErrors rmb_remove_failed
ClearErrors
ExecShell "open" "$PLUGINSDIR\RedmibookManagerMaintenance.exe" '--maintain "$INSTDIR\bin" "$RmbMaintenance"' SW_HIDE
IfErrors rmb_remove_failed
StrCpy $0 0
rmb_remove_wait:
IfFileExists "$RmbMaintenance.ready" rmb_remove_service
IfFileExists "$RmbMaintenance.done" rmb_remove_failed
Sleep 100
IntOp $0 $0 + 1
IntCmp $0 300 rmb_remove_failed rmb_remove_wait rmb_remove_failed
rmb_remove_service:
nsExec::ExecToLog '"$INSTDIR\bin\redmibook-service.exe" --uninstall'
Pop $0
StrCmp $0 "0" rmb_remove_commit
Goto rmb_remove_failed
rmb_remove_commit:
FileOpen $0 "$RmbMaintenance.remove" w
FileWrite $0 "remove"
FileClose $0
; Keep the plugin directory and its completion marker alive until maintenance
; acknowledges removal. Otherwise cleanup can erase .remove before it is read.
StrCpy $1 0
rmb_remove_finish_wait:
IfFileExists "$RmbMaintenance.done" rmb_remove_finish_check
Sleep 100
IntOp $1 $1 + 1
IntCmp $1 300 rmb_remove_failed rmb_remove_finish_wait rmb_remove_failed
rmb_remove_finish_check:
FileOpen $0 "$RmbMaintenance.done" r
FileRead $0 $1
FileClose $0
StrCmp $1 "0" rmb_remove_registration rmb_remove_failed
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
