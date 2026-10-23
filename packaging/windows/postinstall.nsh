DetailPrint "$(RmbService)"
StrCmp $RmbMaintenance "" rmb_fresh_service
nsExec::ExecToLog '"$INSTDIR\bin\redmibook-service.exe" --configure'
Pop $0
Goto rmb_service_check
rmb_fresh_service:
nsExec::ExecToLog '"$INSTDIR\bin\redmibook-service.exe" --install'
Pop $0
rmb_service_check:
StrCmp $0 "0" rmb_service_ready
MessageBox MB_OK|MB_ICONSTOP "$(RmbServiceError)" /SD IDOK
StrCmp $RmbMaintenance "" +5
FileOpen $0 "$RmbMaintenance.commit.tmp" w
FileWrite $0 "cancel"
FileClose $0
Rename "$RmbMaintenance.commit.tmp" "$RmbMaintenance.commit"
SetErrorLevel 2
Abort
rmb_service_ready:
DetailPrint "$(RmbAutostart)"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KobaProduction.RedmibookManager.Session" '$\"$INSTDIR\bin\redmibook-session.exe$\" --with-tray'
StrCmp $RmbMaintenance "" rmb_resume_done
DetailPrint "$(RmbRestore)"
FileOpen $0 "$RmbMaintenance.commit.tmp" w
FileWrite $0 "commit"
FileClose $0
Rename "$RmbMaintenance.commit.tmp" "$RmbMaintenance.commit"
StrCpy $1 0
rmb_resume_wait:
IfFileExists "$RmbMaintenance.done" rmb_resume_check
Sleep 100
IntOp $1 $1 + 1
IntCmp $1 300 rmb_resume_failed rmb_resume_wait rmb_resume_failed
rmb_resume_check:
FileOpen $0 "$RmbMaintenance.done" r
FileRead $0 $1
FileClose $0
StrCmp $1 "0" rmb_resume_done
rmb_resume_failed:
MessageBox MB_OK|MB_ICONSTOP "$(RmbRestoreError)" /SD IDOK
SetErrorLevel 2
Abort
rmb_resume_done:
DetailPrint "$(RmbReady)"
