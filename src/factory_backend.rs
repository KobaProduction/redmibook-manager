use crate::capability::{CapabilityDescriptor, DisplayConfiguration, PerformanceProfile, FACTORY_CAPABILITIES};
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

    pub fn read_charge_protection_80(
        &self,
    ) -> Result<bool, FactoryBackendError<T::Error>> {
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

    fn call(
        &self,
        request: MifsRequest,
    ) -> Result<MifsResponse, FactoryBackendError<T::Error>> {
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
    use crate::factory_protocol::{MifsFunction, MifsOperation};

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

    #[test]
    fn reads_typed_performance_profile() {
        let backend = FactoryFirmwareBackend::new(
            |request: [u8; MIFS_PACKET_SIZE]| -> Result<[u8; MIFS_PACKET_SIZE], ()> {
                assert_eq!(request[1], MifsOperation::Get as u8);
                assert_eq!(request[3], MifsFunction::PerformanceProfile as u8);
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
                assert_eq!(request[1], MifsOperation::Set as u8);
                assert_eq!(request[3], MifsFunction::BatteryControl as u8);
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
