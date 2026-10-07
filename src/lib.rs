//! Core semantic contracts for RedmiBook Manager.
//!
//! UI code must depend on the semantic types in this crate, not on raw
//! firmware selectors, EC offsets, GUIDs, or transport-specific packet fields.

pub mod backend;
pub mod capability;
pub mod factory_backend;
pub mod factory_protocol;
pub mod factory_telemetry;
pub mod windows_wmi_contract;
#[cfg(windows)]
pub mod windows_wmi_transport;

pub use backend::{BackendKind, CoolingTelemetryBackend, PlatformBackend};
pub use capability::{
    CapabilityAvailability, CapabilityDescriptor, CapabilityId, CapabilityReadiness,
    CpuFanSpeedRpm, DisplayConfiguration, FanModeSelection, KeyboardBacklightPolicy,
    PerformanceProfile,
    TurboFanSpeedSelection, UsbChargeMode, UsbChargeThresholdPercent, FACTORY_CAPABILITIES,
};
pub use factory_backend::{
    FactoryBackendError, FactoryFirmwareBackend, FactoryMicrophoneMuteSignal, FactoryWmiTransport,
};

pub use factory_telemetry::{
    FactoryCoolingTelemetryBackend, FactoryPtIdTransport, FactoryTelemetryError,
};

pub use windows_wmi_contract::{
    WindowsWmiSchema, FACTORY_WINDOWS_WMI_SCHEMA, WINDOWS_WMI_INSTANCE_QUERY,
};
#[cfg(windows)]
pub use windows_wmi_transport::{WindowsMifsWmiError, WindowsMifsWmiTransport};
