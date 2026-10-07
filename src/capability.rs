#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum CapabilityId {
    FirmwareIdentity,
    PerformanceProfile,
    BatteryChargeProtection80,
    CpuFan1SpeedRpm,
    KeyboardBacklightState,
    KeyboardBacklightPolicy,
    MicrophoneMuteState,
    DisplayConfiguration,
    UsbChargeMode,
    UsbChargeThreshold,
    InternalKeyboardWake,
    WakeOnUsb,
    AutoFanPreset,
    TurboFanSpeedPreset,
    CoolingTelemetry,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CapabilityAvailability {
    RuntimeReadOnly,
    RuntimeReadWrite,
    PersistedFirmwarePolicy,
    TransportPending,
    NotPublished,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CapabilityReadiness {
    ContractReady,
    Partial,
    FutureFirmware,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CapabilityDescriptor {
    pub id: CapabilityId,
    pub availability: CapabilityAvailability,
    pub readiness: CapabilityReadiness,
    pub same_model_execution_corroborated: bool,
    pub local_execution_proof: bool,
}

pub const FACTORY_CAPABILITIES: [CapabilityDescriptor; 15] = [
    CapabilityDescriptor {
        id: CapabilityId::FirmwareIdentity,
        availability: CapabilityAvailability::RuntimeReadOnly,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: true,
        local_execution_proof: true,
    },
    CapabilityDescriptor {
        id: CapabilityId::PerformanceProfile,
        availability: CapabilityAvailability::RuntimeReadWrite,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: true,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::BatteryChargeProtection80,
        availability: CapabilityAvailability::RuntimeReadWrite,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: true,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::CpuFan1SpeedRpm,
        availability: CapabilityAvailability::TransportPending,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::KeyboardBacklightState,
        availability: CapabilityAvailability::RuntimeReadOnly,
        readiness: CapabilityReadiness::Partial,
        same_model_execution_corroborated: true,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::KeyboardBacklightPolicy,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::MicrophoneMuteState,
        availability: CapabilityAvailability::NotPublished,
        readiness: CapabilityReadiness::Partial,
        same_model_execution_corroborated: true,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::DisplayConfiguration,
        availability: CapabilityAvailability::TransportPending,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: true,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::UsbChargeMode,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::UsbChargeThreshold,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::InternalKeyboardWake,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::WakeOnUsb,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::ContractReady,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::AutoFanPreset,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::Partial,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::TurboFanSpeedPreset,
        availability: CapabilityAvailability::PersistedFirmwarePolicy,
        readiness: CapabilityReadiness::Partial,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
    CapabilityDescriptor {
        id: CapabilityId::CoolingTelemetry,
        availability: CapabilityAvailability::NotPublished,
        readiness: CapabilityReadiness::Partial,
        same_model_execution_corroborated: false,
        local_execution_proof: false,
    },
];

pub fn factory_capability(id: CapabilityId) -> Option<&'static CapabilityDescriptor> {
    FACTORY_CAPABILITIES.iter().find(|descriptor| descriptor.id == id)
}

#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PerformanceProfile {
    Balanced = 1,
    Quiet = 2,
    Turbo = 3,
    FullSpeed = 4,
}

impl TryFrom<u16> for PerformanceProfile {
    type Error = u16;

    fn try_from(value: u16) -> Result<Self, Self::Error> {
        match value {
            1 => Ok(Self::Balanced),
            2 => Ok(Self::Quiet),
            3 => Ok(Self::Turbo),
            4 => Ok(Self::FullSpeed),
            other => Err(other),
        }
    }
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyboardBacklightPolicy {
    Standard = 0,
    PowerSaving = 1,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbChargeMode {
    Off = 0,
    AlwaysOn = 1,
    OneTimeOnly = 2,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UsbChargeThresholdPercent {
    Percent10 = 10,
    Percent20 = 20,
    Percent30 = 30,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FanModeSelection {
    Gaming = 0,
    Normal = 1,
    Office = 2,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TurboFanSpeedSelection {
    Max = 0,
    Medium = 1,
}

/// Factory firmware exposes a binary Display Configuration field, but the
/// user-facing meaning of states 0 and 1 is not yet closed.
#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DisplayConfiguration {
    State0 = 0,
    State1 = 1,
}

impl TryFrom<u16> for DisplayConfiguration {
    type Error = u16;

    fn try_from(value: u16) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::State0),
            1 => Ok(Self::State1),
            other => Err(other),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn performance_profile_mapping_is_tm2309_specific() {
        assert_eq!(PerformanceProfile::try_from(1), Ok(PerformanceProfile::Balanced));
        assert_eq!(PerformanceProfile::try_from(2), Ok(PerformanceProfile::Quiet));
        assert_eq!(PerformanceProfile::try_from(3), Ok(PerformanceProfile::Turbo));
        assert_eq!(PerformanceProfile::try_from(4), Ok(PerformanceProfile::FullSpeed));
        assert_eq!(PerformanceProfile::try_from(0), Err(0));
        assert_eq!(PerformanceProfile::try_from(5), Err(5));
    }

    #[test]
    fn factory_capability_lookup_uses_semantic_ids() {
        let profile = factory_capability(CapabilityId::PerformanceProfile).unwrap();
        assert_eq!(profile.availability, CapabilityAvailability::RuntimeReadWrite);
        assert_eq!(profile.readiness, CapabilityReadiness::ContractReady);

        let telemetry = factory_capability(CapabilityId::CoolingTelemetry).unwrap();
        assert_eq!(telemetry.availability, CapabilityAvailability::NotPublished);
        assert_eq!(telemetry.readiness, CapabilityReadiness::Partial);

        let backlight_policy =
            factory_capability(CapabilityId::KeyboardBacklightPolicy).unwrap();
        assert_eq!(
            backlight_policy.availability,
            CapabilityAvailability::PersistedFirmwarePolicy
        );
        assert_eq!(
            backlight_policy.readiness,
            CapabilityReadiness::ContractReady
        );
    }

    #[test]
    fn factory_capability_ids_are_unique() {
        for (index, left) in FACTORY_CAPABILITIES.iter().enumerate() {
            for right in &FACTORY_CAPABILITIES[index + 1..] {
                assert_ne!(left.id, right.id);
            }
        }
    }

    #[test]
    fn setup_policy_values_are_semantic_not_raw_ui_indices() {
        assert_eq!(KeyboardBacklightPolicy::Standard as u8, 0);
        assert_eq!(KeyboardBacklightPolicy::PowerSaving as u8, 1);
        assert_eq!(UsbChargeMode::Off as u8, 0);
        assert_eq!(UsbChargeMode::AlwaysOn as u8, 1);
        assert_eq!(UsbChargeMode::OneTimeOnly as u8, 2);
        assert_eq!(FanModeSelection::Gaming as u8, 0);
        assert_eq!(FanModeSelection::Normal as u8, 1);
        assert_eq!(FanModeSelection::Office as u8, 2);
    }
}
