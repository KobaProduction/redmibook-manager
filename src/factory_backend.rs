use crate::backend::{BackendKind, PlatformBackend};
use crate::capability::{
    CapabilityDescriptor, DisplayConfiguration, PerformanceProfile, FACTORY_CAPABILITIES,
};
use crate::factory_protocol::{
    BatteryControlSubcommand, MifsRequest, MifsResponse, MIFS_PACKET_SIZE, MIFS_STATUS_SUCCESS,
};

pub trait FactoryWmiTransport {
    type Error;

    fn invoke_wmaa(
        &self,
        request: [u8; MIFS_PACKET_SIZE],
    ) -> Result<[u8; MIFS_PACKET_SIZE], Self::Error>;
}

impl<F, E> FactoryWmiTransport for F
where
    F: Fn([u8; MIFS_PACKET_SIZE]) -> Result<[u8; MIFS_PACKET_SIZE], E>,
{
    type Error = E;

    fn invoke_wmaa(
        &self,
        request: [u8; MIFS_PACKET_SIZE],
    ) -> Result<[u8; MIFS_PACKET_SIZE], Self::Error> {
        (self)(request)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum FactoryBackendError<E> {
    Transport(E),
    FirmwareStatus(u16),
    InvalidPerformanceProfile(u16),
    InvalidBoolean(u32),
    InvalidDisplayConfiguration(u16),
    InvalidMicrophoneMuteSignal(u32),
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FactoryMicrophoneMuteSignal {
    State0 = 0,
    State1 = 1,
}

impl TryFrom<u32> for FactoryMicrophoneMuteSignal {
    type Error = u32;

    fn try_from(value: u32) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::State0),
            1 => Ok(Self::State1),
            other => Err(other),
        }
    }
}

/// Raw factory WMI telemetry group 0x0900.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FactoryRawTelemetry0900 {
    pub value0: u16,
    pub value1: u32,
    pub value2: u32,
    pub value3: u32,
}

pub struct FactoryFirmwareBackend<T> {
    transport: T,
}

impl<T> FactoryFirmwareBackend<T>
where
    T: FactoryWmiTransport,
{
    pub fn new(transport: T) -> Self {
        Self { transport }
    }

    pub fn capabilities(&self) -> &'static [CapabilityDescriptor] {
        &FACTORY_CAPABILITIES
    }

    pub fn read_performance_profile(
        &self,
    ) -> Result<PerformanceProfile, FactoryBackendError<T::Error>> {
        let response = self.call(MifsRequest::read_performance_profile())?;
        PerformanceProfile::try_from(response.value0_u16())
            .map_err(FactoryBackendError::InvalidPerformanceProfile)
    }

    pub fn write_performance_profile(
        &self,
        profile: PerformanceProfile,
    ) -> Result<(), FactoryBackendError<T::Error>> {
        self.call(MifsRequest::write_performance_profile(profile))?;
        Ok(())
    }

    pub fn read_charge_protection_80(&self) -> Result<bool, FactoryBackendError<T::Error>> {
        let response = self.call(MifsRequest::read_battery(
            BatteryControlSubcommand::ChargeProtection80,
        ))?;

        match response.value1_u32() {
            0 => Ok(false),
            1 => Ok(true),
            other => Err(FactoryBackendError::InvalidBoolean(other)),
        }
    }

    pub fn write_charge_protection_80(
        &self,
        enabled: bool,
    ) -> Result<(), FactoryBackendError<T::Error>> {
        self.call(MifsRequest::write_charge_protection_80(enabled))?;
        Ok(())
    }

    /// Read the factory firmware's outward microphone-mute boolean signal.
    ///
    /// The signal is intentionally neutral (State0/State1) until local
    /// execution proof establishes which value is user-facing muted/unmuted.
    pub fn read_microphone_mute_signal(
        &self,
    ) -> Result<FactoryMicrophoneMuteSignal, FactoryBackendError<T::Error>> {
        let response = self.call(MifsRequest::read_microphone_mute_signal())?;
        FactoryMicrophoneMuteSignal::try_from(response.value1_u32())
            .map_err(FactoryBackendError::InvalidMicrophoneMuteSignal)
    }

    /// Write the factory firmware's outward microphone-mute boolean signal.
    ///
    /// This method exists for execution validation and is deliberately not part
    /// of the semantic PlatformBackend surface yet.
    pub fn write_microphone_mute_signal(
        &self,
        signal: FactoryMicrophoneMuteSignal,
    ) -> Result<(), FactoryBackendError<T::Error>> {
        self.call(MifsRequest::write_microphone_mute_signal(
            signal == FactoryMicrophoneMuteSignal::State1,
        ))?;
        Ok(())
    }

    pub fn read_display_configuration(
        &self,
    ) -> Result<DisplayConfiguration, FactoryBackendError<T::Error>> {
        let response = self.call(MifsRequest::read_display_configuration())?;
        DisplayConfiguration::try_from(response.value0_u16())
            .map_err(FactoryBackendError::InvalidDisplayConfiguration)
    }

    pub fn write_display_configuration(
        &self,
        state: DisplayConfiguration,
    ) -> Result<(), FactoryBackendError<T::Error>> {
        self.call(MifsRequest::write_display_configuration(state))?;
        Ok(())
    }

    /// Read the factory WMI selector group 0x0900 without assigning product
    /// meaning to the four returned values.
    ///
    /// This is intentionally a factory-specific diagnostic API used to obtain
    /// execution proof. It is not part of PlatformBackend and must not be
    /// surfaced as CPU/GPU temperature or fan telemetry until runtime
    /// correlation proves the mapping.
    pub fn read_raw_telemetry_0900(
        &self,
    ) -> Result<FactoryRawTelemetry0900, FactoryBackendError<T::Error>> {
        let response = self.call(MifsRequest::read_telemetry_0900())?;
        Ok(FactoryRawTelemetry0900 {
            value0: response.value0_u16(),
            value1: response.value1_u32(),
            value2: response.value2_u32(),
            value3: response.value3_u32(),
        })
    }

    fn call(&self, request: MifsRequest) -> Result<MifsResponse, FactoryBackendError<T::Error>> {
        let raw = self
            .transport
            .invoke_wmaa(request.into_bytes())
            .map_err(FactoryBackendError::Transport)?;

        let response = MifsResponse::from_bytes(raw);
        if response.status() != MIFS_STATUS_SUCCESS {
            return Err(FactoryBackendError::FirmwareStatus(response.status()));
        }

        // TM2309 SET operations may succeed without echoing the function ID.
        // Success is therefore status-driven; semantic validation happens when
        // reading typed return values.
        Ok(response)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::factory_protocol::{MifsOperation, MifsSelector};

    fn success_with_value0(value: u16) -> [u8; MIFS_PACKET_SIZE] {
        let mut bytes = [0u8; MIFS_PACKET_SIZE];
        bytes[0..2].copy_from_slice(&MIFS_STATUS_SUCCESS.to_le_bytes());
        bytes[4..6].copy_from_slice(&value.to_le_bytes());
        bytes
    }

    fn success_with_value1(value: u32) -> [u8; MIFS_PACKET_SIZE] {
        let mut bytes = [0u8; MIFS_PACKET_SIZE];
        bytes[0..2].copy_from_slice(&MIFS_STATUS_SUCCESS.to_le_bytes());
        bytes[6..10].copy_from_slice(&value.to_le_bytes());
        bytes
    }

    fn request_operation(request: &[u8; MIFS_PACKET_SIZE]) -> u16 {
        u16::from_le_bytes([request[0], request[1]])
    }

    fn request_selector(request: &[u8; MIFS_PACKET_SIZE]) -> u16 {
        u16::from_le_bytes([request[2], request[3]])
    }

    #[test]
    fn reads_typed_performance_profile() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Get as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::PerformanceProfile as u16
                );
                Ok(success_with_value0(3))
            },
        );

        assert_eq!(
            backend.read_performance_profile(),
            Ok(PerformanceProfile::Turbo)
        );
    }

    #[test]
    fn writes_charge_protection_through_tm2309_specific_function_10() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Set as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::BatteryControl as u16
                );
                assert_eq!(u16::from_le_bytes([request[4], request[5]]), 2);
                assert_eq!(
                    u32::from_le_bytes([request[6], request[7], request[8], request[9]]),
                    1
                );
                Ok(success_with_value1(0))
            },
        );

        assert_eq!(backend.write_charge_protection_80(true), Ok(()));
    }

    #[test]
    fn microphone_signal_transport_stays_polarity_neutral() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Get as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::MicrophoneControl as u16
                );
                assert_eq!(u16::from_le_bytes([request[4], request[5]]), 5);
                Ok(success_with_value1(1))
            },
        );

        assert_eq!(
            backend.read_microphone_mute_signal(),
            Ok(FactoryMicrophoneMuteSignal::State1)
        );
    }

    #[test]
    fn writes_neutral_microphone_signal_without_claiming_user_polarity() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Set as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::MicrophoneControl as u16
                );
                assert_eq!(u16::from_le_bytes([request[4], request[5]]), 5);
                assert_eq!(
                    u32::from_le_bytes([request[6], request[7], request[8], request[9]]),
                    0
                );
                Ok(success_with_value1(0))
            },
        );

        assert_eq!(
            backend.write_microphone_mute_signal(FactoryMicrophoneMuteSignal::State0),
            Ok(())
        );
    }

    #[test]
    fn reads_raw_telemetry_0900_without_claiming_semantics() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Get as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::TelemetryGroup0900 as u16
                );

                let mut bytes = [0u8; MIFS_PACKET_SIZE];
                bytes[0..2].copy_from_slice(&MIFS_STATUS_SUCCESS.to_le_bytes());
                bytes[4..6].copy_from_slice(&11u16.to_le_bytes());
                bytes[6..10].copy_from_slice(&22u32.to_le_bytes());
                bytes[10..14].copy_from_slice(&33u32.to_le_bytes());
                bytes[14..18].copy_from_slice(&44u32.to_le_bytes());
                Ok(bytes)
            },
        );

        assert_eq!(
            backend.read_raw_telemetry_0900(),
            Ok(FactoryRawTelemetry0900 {
                value0: 11,
                value1: 22,
                value2: 33,
                value3: 44,
            })
        );
    }

    #[test]
    fn accepts_successful_set_without_function_echo() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request_operation(&request), MifsOperation::Set as u16);
                assert_eq!(
                    request_selector(&request),
                    MifsSelector::PerformanceProfile as u16
                );

                let mut bytes = [0u8; MIFS_PACKET_SIZE];
                bytes[0..2].copy_from_slice(&MIFS_STATUS_SUCCESS.to_le_bytes());
                // TM2309 may leave returned_function at zero on successful SET.
                Ok(bytes)
            },
        );

        assert_eq!(
            backend.write_performance_profile(PerformanceProfile::Quiet),
            Ok(())
        );
    }

    #[test]
    fn rejects_unknown_firmware_status() {
        let backend = FactoryFirmwareBackend::new(
            |_request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                let mut bytes = [0u8; MIFS_PACKET_SIZE];
                bytes[0..2].copy_from_slice(&0xE000u16.to_le_bytes());
                Ok(bytes)
            },
        );

        assert_eq!(
            backend.read_performance_profile(),
            Err(FactoryBackendError::FirmwareStatus(0xE000))
        );
    }
}

impl<T> PlatformBackend for FactoryFirmwareBackend<T>
where
    T: FactoryWmiTransport,
{
    type Error = FactoryBackendError<T::Error>;

    fn kind(&self) -> BackendKind {
        BackendKind::FactoryFirmware
    }

    fn capabilities(&self) -> &'static [CapabilityDescriptor] {
        FactoryFirmwareBackend::capabilities(self)
    }

    fn read_performance_profile(&self) -> Result<PerformanceProfile, Self::Error> {
        FactoryFirmwareBackend::read_performance_profile(self)
    }

    fn write_performance_profile(&self, profile: PerformanceProfile) -> Result<(), Self::Error> {
        FactoryFirmwareBackend::write_performance_profile(self, profile)
    }

    fn read_charge_protection_80(&self) -> Result<bool, Self::Error> {
        FactoryFirmwareBackend::read_charge_protection_80(self)
    }

    fn write_charge_protection_80(&self, enabled: bool) -> Result<(), Self::Error> {
        FactoryFirmwareBackend::write_charge_protection_80(self, enabled)
    }

    fn read_display_configuration(&self) -> Result<DisplayConfiguration, Self::Error> {
        FactoryFirmwareBackend::read_display_configuration(self)
    }

    fn write_display_configuration(&self, state: DisplayConfiguration) -> Result<(), Self::Error> {
        FactoryFirmwareBackend::write_display_configuration(self, state)
    }
}
