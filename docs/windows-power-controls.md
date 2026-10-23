# Windows brightness and energy saver

`application/display/brightness_port.hpp` owns internal-panel brightness.
`application/power/saver_port.hpp` owns manual energy-saver control and observation.
Platform factories are selected in CMake. Linux currently assembles neither module;
absent capabilities contribute no controls or preset actions. Manager shares the
same native adapter instances between its controls and preset executor.

Brightness uses ROOT/WMI WmiMonitorBrightness and WmiMonitorBrightnessMethods.
Only a unique active instance is accepted. WmiSetBrightness uses zero transition
latency; semisynchronous completion uses IWbemCallResult::GetCallStatus. Its final
HRESULT and CurrentBrightness readback must both confirm the requested value.
No sleeps, result-object assumptions or API-success-only confirmation are used.
Outstanding WMI results are retained in the COM Global Interface Table so later
worker apartments can check completion without reissuing the write. A process-wide
write gate serializes adapter instances and blocks SET after an unknown outcome.
Only confirmed native completion releases the gate; losing the result handle leaves
it blocked. Reconciliation never assumes that a matching brightness value cancels
an outstanding request. This gate is process-local, not a cross-process arbiter.

Energy saver is the Windows Quick Settings feature, independent of power schemes,
firmware performance profiles and Windows efficiency power modes. Observe the real
Windows 11 24H2 GUID_ENERGY_SAVER_STATUS notification, including its initial state.
The legacy GetSystemPowerStatus battery-saver flag does not represent every new
energy-saver level. Subscription parameters and callback context remain owned until
unregistration; hidden UI does not require repeated native polling of this state.
The application port exposes an RAII observation subscription. The single native
observer fans out real state changes; callbacks only enqueue asynchronous model reads.
Detaching a subscription drains callbacks before its QObject consumer is destroyed.
One model-owned ten-second reconciliation task recovers missed events; pages do not
own polling timers. Unchanged snapshots publish no UI change, and background reads
never set command-busy/disable the energy-saver switch. Explicit writes still have
bounded confirmation and command-busy feedback. Notifications arriving during a request
are coalesced into a follow-up read; a user command queues behind a background read.
Native saver reconciliation uses a separate bounded initial-notification snapshot
from Windows and repairs the long-lived callback cache. An unavailable fresh snapshot
is reported as unavailable rather than silently substituted with cached state.

Manual control uses the internal Windows WNF_PO_ENERGY_SAVER_OVERRIDE route through
NtQueryWnfStateData/NtUpdateWnfStateData. This is NOT a documented public setter.
The tested override payload is one DWORD: 1 requests on, 2 requests off. Enable the
capability only when both native functions exist, the observed state is available,
and the queried payload has the qualified size and values. Compare the change
stamp when writing, so concurrent manual changes are not blindly overwritten.
After an accepted request, wait for the requested GUID_ENERGY_SAVER_STATUS state
with a bounded native event wait on a worker. A timeout or write conflict is
Unconfirmed, access denial is Denied, and missing qualification omits the control.
An accepted WNF update alone never proves that energy saver became active.
The native confirmation wait blocks on its notification event with a deadline on a
worker; it is not a polling or busy loop and performs no repeated hardware query.

Native on/off transitions were qualified on Windows 11 25H2 build 26200.6899.
Other builds must satisfy runtime qualification and the same observed-result check;
this internal route is not a Windows compatibility guarantee. A rejected or
unconfirmed request stops subsequent preset actions. Operations use no elevation,
UI automation, registry writes, policy changes or automatic activation thresholds.
The normal Windows service may persist its own manual state as usual.

The owner's manual-only requirement excludes automatic battery thresholds and
administrative always-on policies. Preserve existing actions and stable preset IDs:
firmware profile, internal-panel refresh, Windows plan, manual saver and brightness.
Device shows battery telemetry first, followed by one charge-limit/manual-saver card.
Brightness remains a preset action; Device does not duplicate the Windows brightness slider.
The explicit diagnostic `brightness-probe --saver-round-trip` toggles and restores
the initially observed state; it is not an automatically run hardware CTest.

References:

- https://learn.microsoft.com/en-us/windows/win32/wmicoreprov/wmimonitorbrightnessmethods
- https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemcallresult-getcallstatus
- https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/powersettingregisternotification/
- https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/winnt.h
- https://github.com/ionescu007/wnfun/blob/master/script_python/WellKnownWnfNames.py
- https://github.com/winsiderss/systeminformer/blob/master/phnt/include/ntexapi.h
