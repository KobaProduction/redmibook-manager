SetShellVarContext all
InitPluginsDir
StrCpy $RmbMaintenance ""
DetailPrint "$(RmbPrepare)"
IfFileExists "$INSTDIR\bin\redmibook-manager.exe" 0 rmb_prepare_done
SetOutPath "$PLUGINSDIR"
File /oname=RedmibookManagerMaintenance.exe "${INST_DIR}\bin\redmibook-service.exe"
StrCpy $RmbMaintenance "$PLUGINSDIR\rmb-update"
ExecShell "open" "$PLUGINSDIR\RedmibookManagerMaintenance.exe" '--maintain "$INSTDIR\bin" "$RmbMaintenance"' SW_HIDE
IfErrors rmb_prepare_failed
StrCpy $0 0
rmb_prepare_wait:
IfFileExists "$RmbMaintenance.ready" rmb_prepare_done
IfFileExists "$RmbMaintenance.done" rmb_prepare_failed
Sleep 100
IntOp $0 $0 + 1
IntCmp $0 300 rmb_prepare_failed rmb_prepare_wait rmb_prepare_failed
rmb_prepare_failed:
FileOpen $0 "$RmbMaintenance.commit.tmp" w
FileWrite $0 "cancel"
FileClose $0
Rename "$RmbMaintenance.commit.tmp" "$RmbMaintenance.commit"
MessageBox MB_OK|MB_ICONSTOP "$(RmbPrepareError)" /SD IDOK
SetErrorLevel 2
Abort
rmb_prepare_done:
DetailPrint "$(RmbCopy)"
SetOutPath "$INSTDIR"
