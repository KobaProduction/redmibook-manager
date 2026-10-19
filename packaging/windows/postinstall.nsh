StrCmp $RmbMaintenance "" rmb_fresh_service
ExecWait '"$INSTDIR\bin\redmibook-service.exe" --configure' $0
Goto rmb_service_check
rmb_fresh_service:
ExecWait '"$INSTDIR\bin\redmibook-service.exe" --install' $0
rmb_service_check:
IntCmp $0 0 rmb_service_ready
MessageBox MB_OK|MB_ICONSTOP "The diagnostic service could not be installed or started." /SD IDOK
StrCmp $RmbMaintenance "" +5
FileOpen $0 "$RmbMaintenance.commit.tmp" w
FileWrite $0 "cancel"
FileClose $0
Rename "$RmbMaintenance.commit.tmp" "$RmbMaintenance.commit"
SetErrorLevel 2
Abort
rmb_service_ready:
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "KobaProduction.RedmibookManager.Session" '$\"$INSTDIR\bin\redmibook-session.exe$\" --with-tray'
StrCmp $RmbMaintenance "" rmb_resume_done
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
MessageBox MB_OK|MB_ICONSTOP "Files were installed, but background restart could not be verified. Open Manager to inspect diagnostics." /SD IDOK
SetErrorLevel 2
Abort
rmb_resume_done:
