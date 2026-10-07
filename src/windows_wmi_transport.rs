#![cfg(windows)]

use serde::{Deserialize, Serialize};
use wmi::{WMIConnection, WMIError};

use crate::factory_backend::FactoryWmiTransport;
use crate::factory_protocol::{
    MifsResponse, MIFS_PACKET_SIZE, WINDOWS_WMI_METHOD, WINDOWS_WMI_NAMESPACE,
    WINDOWS_WMI_OUT_DATA_SIZE,
};
use crate::windows_wmi_contract::WINDOWS_WMI_INSTANCE_QUERY;

#[derive(Debug)]
pub enum WindowsMifsWmiError {
    Wmi(WMIError),
    NoActiveInstance,
    InvalidOutDataLength(usize),
}

impl From<WMIError> for WindowsMifsWmiError {
    fn from(value: WMIError) -> Self {
        Self::Wmi(value)
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename = "MICommonInterface")]
struct MiCommonInterface {
    #[serde(rename = "__Path")]
    path: String,
    #[serde(rename = "InstanceName")]
    instance_name: String,
    #[serde(rename = "Active")]
    active: bool,
}

#[derive(Debug, Serialize)]
struct MiInterfaceInput {
    #[serde(rename = "InData")]
    in_data: Vec<u8>,
}

#[derive(Debug, Deserialize)]
struct MiInterfaceOutput {
    #[serde(rename = "OutData")]
    out_data: Vec<u8>,
    #[serde(rename = "Reserved")]
    reserved: u16,
}

/// Concrete Windows user-mode transport for the factory ACPI-WMI control
/// interface.
///
/// The adapter is intentionally narrow: it discovers one active
/// MICommonInterface instance and invokes only the confirmed MiInterface method
/// with the fixed 32-byte TM2309 packet. It does not expose arbitrary WMI or
/// ACPI method execution to the application layer.
pub struct WindowsMifsWmiTransport {
    connection: WMIConnection,
    instance_path: String,
    instance_name: String,
}

impl WindowsMifsWmiTransport {
    pub fn connect() -> Result<Self, WindowsMifsWmiError> {
        let connection = WMIConnection::with_namespace_path(WINDOWS_WMI_NAMESPACE)?;
        let instances: Vec<MiCommonInterface> =
            connection.raw_query(WINDOWS_WMI_INSTANCE_QUERY)?;

        let instance = instances
            .into_iter()
            .find(|instance| instance.active)
            .ok_or(WindowsMifsWmiError::NoActiveInstance)?;

        Ok(Self {
            connection,
            instance_path: instance.path,
            instance_name: instance.instance_name,
        })
    }

    pub fn instance_name(&self) -> &str {
        &self.instance_name
    }

    fn call_mi_interface(
        &self,
        request: [u8; MIFS_PACKET_SIZE],
    ) -> Result<[u8; MIFS_PACKET_SIZE], WindowsMifsWmiError> {
        let input = MiInterfaceInput {
            in_data: request.to_vec(),
        };

        let output: MiInterfaceOutput = self
            .connection
            .exec_instance_method::<MiCommonInterface, _>(
                &self.instance_path,
                WINDOWS_WMI_METHOD,
                &input,
            )?;

        let out_len = output.out_data.len();
        let out_data: [u8; WINDOWS_WMI_OUT_DATA_SIZE] = output
            .out_data
            .try_into()
            .map_err(|_| WindowsMifsWmiError::InvalidOutDataLength(out_len))?;

        Ok(MifsResponse::from_windows_wmi_output(out_data, output.reserved).into_bytes())
    }
}

impl FactoryWmiTransport for WindowsMifsWmiTransport {
    type Error = WindowsMifsWmiError;

    fn invoke_wmaa(
        &self,
        request: [u8; MIFS_PACKET_SIZE],
    ) -> Result<[u8; MIFS_PACKET_SIZE], Self::Error> {
        self.call_mi_interface(request)
    }
}
