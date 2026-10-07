use crate::capability::{DisplayConfiguration, PerformanceProfile};

pub const TM2309_WMI_CONTROL_GUID: &str = "B60BFB48-3E5B-49E4-A0E9-8CFFE1B3434B";
pub const MIFS_PACKET_SIZE: usize = 32;
pub const MIFS_STATUS_SUCCESS: u16 = 0x8000;
pub const MIFS_STATUS_ERROR: u16 = 0xE000;

const OPERATION_OFFSET: usize = 1;
const FUNCTION_OFFSET: usize = 3;
const VALUE0_OFFSET: usize = 4;
const VALUE1_OFFSET: usize = 6;
const VALUE2_OFFSET: usize = 10;
const VALUE3_OFFSET: usize = 14;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MifsOperation {
    Get = 0xFA,
    Set = 0xFB,
}

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MifsFunction {
    PerformanceProfile = 0x08,
    MicrophoneControl = 0x0A,
    DisplayConfiguration = 0x0B,
    BatteryControl = 0x10,
}

#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BatteryControlSubcommand {
    RawSoh1 = 1,
    ChargeProtection80 = 2,
    AdapterPowerThresholdStatus = 3,
}

#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MicrophoneControlSubcommand {
    MuteState = 5,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MifsRequest {
    bytes: [u8; MIFS_PACKET_SIZE],
}

impl MifsRequest {
    pub fn new(operation: MifsOperation, function: MifsFunction) -> Self {
        let mut bytes = [0u8; MIFS_PACKET_SIZE];
        bytes[OPERATION_OFFSET] = operation as u8;
        bytes[FUNCTION_OFFSET] = function as u8;
        Self { bytes }
    }

    pub fn with_value0_u16(mut self, value: u16) -> Self {
        self.bytes[VALUE0_OFFSET..VALUE0_OFFSET + 2].copy_from_slice(&value.to_le_bytes());
        self
    }

    pub fn with_value1_u32(mut self, value: u32) -> Self {
        self.bytes[VALUE1_OFFSET..VALUE1_OFFSET + 4].copy_from_slice(&value.to_le_bytes());
        self
    }

    pub fn as_bytes(&self) -> &[u8; MIFS_PACKET_SIZE] {
        &self.bytes
    }

    pub fn into_bytes(self) -> [u8; MIFS_PACKET_SIZE] {
        self.bytes
    }

    pub fn read_performance_profile() -> Self {
        Self::new(MifsOperation::Get, MifsFunction::PerformanceProfile)
    }

    pub fn write_performance_profile(profile: PerformanceProfile) -> Self {
        Self::new(MifsOperation::Set, MifsFunction::PerformanceProfile)
            .with_value0_u16(profile as u16)
    }

    pub fn read_battery(subcommand: BatteryControlSubcommand) -> Self {
        Self::new(MifsOperation::Get, MifsFunction::BatteryControl)
            .with_value0_u16(subcommand as u16)
    }

    pub fn write_charge_protection_80(enabled: bool) -> Self {
        Self::new(MifsOperation::Set, MifsFunction::BatteryControl)
            .with_value0_u16(BatteryControlSubcommand::ChargeProtection80 as u16)
            .with_value1_u32(if enabled { 1 } else { 0 })
    }

    /// Build the confirmed factory microphone-control read request.
    ///
    /// The returned outward 0/1 signal is intentionally not named muted/unmuted
    /// until local execution proof closes the user-facing polarity.
    pub fn read_microphone_mute_signal() -> Self {
        Self::new(MifsOperation::Get, MifsFunction::MicrophoneControl)
            .with_value0_u16(MicrophoneControlSubcommand::MuteState as u16)
    }

    /// Build the confirmed factory microphone-control write request.
    ///
    /// outward_state is the firmware-visible boolean signal, not a claimed
    /// user-facing muted/unmuted value.
    pub fn write_microphone_mute_signal(outward_state: bool) -> Self {
        Self::new(MifsOperation::Set, MifsFunction::MicrophoneControl)
            .with_value0_u16(MicrophoneControlSubcommand::MuteState as u16)
            .with_value1_u32(if outward_state { 1 } else { 0 })
    }

    pub fn read_display_configuration() -> Self {
        Self::new(MifsOperation::Get, MifsFunction::DisplayConfiguration)
    }

    pub fn write_display_configuration(state: DisplayConfiguration) -> Self {
        Self::new(MifsOperation::Set, MifsFunction::DisplayConfiguration)
            .with_value0_u16(state as u16)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MifsResponse {
    bytes: [u8; MIFS_PACKET_SIZE],
}

impl MifsResponse {
    pub fn from_bytes(bytes: [u8; MIFS_PACKET_SIZE]) -> Self {
        Self { bytes }
    }

    pub fn status(&self) -> u16 {
        u16::from_le_bytes([self.bytes[0], self.bytes[1]])
    }

    pub fn returned_function(&self) -> u16 {
        u16::from_le_bytes([self.bytes[2], self.bytes[3]])
    }

    pub fn value0_u16(&self) -> u16 {
        u16::from_le_bytes([self.bytes[VALUE0_OFFSET], self.bytes[VALUE0_OFFSET + 1]])
    }

    pub fn value1_u32(&self) -> u32 {
        read_u32(&self.bytes, VALUE1_OFFSET)
    }

    pub fn value2_u32(&self) -> u32 {
        read_u32(&self.bytes, VALUE2_OFFSET)
    }

    pub fn value3_u32(&self) -> u32 {
        read_u32(&self.bytes, VALUE3_OFFSET)
    }

    pub fn is_success(&self) -> bool {
        self.status() == MIFS_STATUS_SUCCESS
    }
}

fn read_u32(bytes: &[u8; MIFS_PACKET_SIZE], offset: usize) -> u32 {
    u32::from_le_bytes([
        bytes[offset],
        bytes[offset + 1],
        bytes[offset + 2],
        bytes[offset + 3],
    ])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn performance_set_packet_matches_tm2309_layout() {
        let request = MifsRequest::write_performance_profile(PerformanceProfile::Turbo);
        let bytes = request.as_bytes();

        assert_eq!(bytes[0], 0);
        assert_eq!(bytes[1], MifsOperation::Set as u8);
        assert_eq!(bytes[2], 0);
        assert_eq!(bytes[3], MifsFunction::PerformanceProfile as u8);
        assert_eq!(u16::from_le_bytes([bytes[4], bytes[5]]), 3);
    }

    #[test]
    fn charge_protection_set_uses_tm2309_battery_group() {
        let request = MifsRequest::write_charge_protection_80(true);
        let bytes = request.as_bytes();

        assert_eq!(bytes[1], MifsOperation::Set as u8);
        assert_eq!(bytes[3], MifsFunction::BatteryControl as u8);
        assert_eq!(u16::from_le_bytes([bytes[4], bytes[5]]), 2);
        assert_eq!(
            u32::from_le_bytes([bytes[6], bytes[7], bytes[8], bytes[9]]),
            1
        );
    }

    #[test]
    fn microphone_control_uses_confirmed_subcommand_5() {
        let read = MifsRequest::read_microphone_mute_signal();
        assert_eq!(read.as_bytes()[1], MifsOperation::Get as u8);
        assert_eq!(read.as_bytes()[3], MifsFunction::MicrophoneControl as u8);
        assert_eq!(
            u16::from_le_bytes([read.as_bytes()[4], read.as_bytes()[5]]),
            MicrophoneControlSubcommand::MuteState as u16
        );

        let write = MifsRequest::write_microphone_mute_signal(true);
        assert_eq!(write.as_bytes()[1], MifsOperation::Set as u8);
        assert_eq!(write.as_bytes()[3], MifsFunction::MicrophoneControl as u8);
        assert_eq!(
            u16::from_le_bytes([write.as_bytes()[4], write.as_bytes()[5]]),
            MicrophoneControlSubcommand::MuteState as u16
        );
        assert_eq!(
            u32::from_le_bytes([
                write.as_bytes()[6],
                write.as_bytes()[7],
                write.as_bytes()[8],
                write.as_bytes()[9],
            ]),
            1
        );
    }

    #[test]
    fn response_fields_follow_firmware_layout() {
        let mut bytes = [0u8; MIFS_PACKET_SIZE];
        bytes[0..2].copy_from_slice(&MIFS_STATUS_SUCCESS.to_le_bytes());
        bytes[2..4].copy_from_slice(&0x0800u16.to_le_bytes());
        bytes[4..6].copy_from_slice(&3u16.to_le_bytes());
        bytes[6..10].copy_from_slice(&1u32.to_le_bytes());

        let response = MifsResponse::from_bytes(bytes);
        assert!(response.is_success());
        assert_eq!(response.returned_function(), 0x0800);
        assert_eq!(response.value0_u16(), 3);
        assert_eq!(response.value1_u32(), 1);
    }
}
