use crate::capability::{CapabilityDescriptor, CpuFanSpeedRpm, PerformanceProfile};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BackendKind {
    FactoryFirmware,
    OpenFirmware,
}

/// Semantic backend boundary consumed by application/UI layers.
///
/// Raw firmware selectors, GUIDs, EC offsets and transport packet fields must
/// stay behind this interface.
pub trait CoolingTelemetryBackend {
    type Error;

    fn read_cpu_fan1_speed_rpm(&self) -> Result<CpuFanSpeedRpm, Self::Error>;
}

/// Semantic control backend boundary consumed by application/UI layers.
///
/// Cooling telemetry is a separate semantic interface because the factory
/// platform uses a different transport for PTID telemetry than for WMI
/// platform controls.
pub trait PlatformBackend {
    type Error;

    fn kind(&self) -> BackendKind;

    fn capabilities(&self) -> &'static [CapabilityDescriptor];

    fn read_performance_profile(&self) -> Result<PerformanceProfile, Self::Error>;

    fn write_performance_profile(&self, profile: PerformanceProfile) -> Result<(), Self::Error>;

    fn read_charge_protection_80(&self) -> Result<bool, Self::Error>;

    fn write_charge_protection_80(&self, enabled: bool) -> Result<(), Self::Error>;

}
