//! Core semantic contracts for RedmiBook Manager.
//!
//! UI code must depend on the semantic types in this crate, not on raw
//! firmware selectors, EC offsets, GUIDs, or transport-specific packet fields.

pub mod backend;
pub mod capability;
pub mod factory_backend;
pub mod factory_protocol;

pub use backend::{BackendKind, PlatformBackend};
pub use capability::{
    CapabilityAvailability, CapabilityDescriptor, CapabilityId, CapabilityReadiness,
    DisplayConfiguration, FanModeSelection, KeyboardBacklightPolicy, PerformanceProfile,
    TurboFanSpeedSelection, UsbChargeMode, UsbChargeThresholdPercent, FACTORY_CAPABILITIES,
};
pub use factory_backend::{
    FactoryBackendError, FactoryFirmwareBackend, FactoryMicrophoneMuteSignal, FactoryWmiTransport,
};
