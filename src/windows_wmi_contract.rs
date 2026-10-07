//! Windows-facing schema contract for the factory ACPI-WMI control surface.
//!
//! This module intentionally contains no COM implementation. It records the
//! exact WMI schema that a Windows adapter must use, while keeping WMI details
//! outside the semantic application/backend API.

use crate::factory_protocol::{
    WINDOWS_WMI_ACTIVE_PROPERTY, WINDOWS_WMI_CLASS, WINDOWS_WMI_INPUT_PROPERTY,
    WINDOWS_WMI_INSTANCE_PROPERTY, WINDOWS_WMI_METHOD, WINDOWS_WMI_NAMESPACE,
    WINDOWS_WMI_OUTPUT_PROPERTY, WINDOWS_WMI_RESERVED_PROPERTY,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct WindowsWmiSchema {
    pub namespace: &'static str,
    pub class: &'static str,
    pub method: &'static str,
    pub instance_property: &'static str,
    pub active_property: &'static str,
    pub input_property: &'static str,
    pub output_property: &'static str,
    pub reserved_property: &'static str,
}

pub const FACTORY_WINDOWS_WMI_SCHEMA: WindowsWmiSchema = WindowsWmiSchema {
    namespace: WINDOWS_WMI_NAMESPACE,
    class: WINDOWS_WMI_CLASS,
    method: WINDOWS_WMI_METHOD,
    instance_property: WINDOWS_WMI_INSTANCE_PROPERTY,
    active_property: WINDOWS_WMI_ACTIVE_PROPERTY,
    input_property: WINDOWS_WMI_INPUT_PROPERTY,
    output_property: WINDOWS_WMI_OUTPUT_PROPERTY,
    reserved_property: WINDOWS_WMI_RESERVED_PROPERTY,
};

/// Discover an active instance at runtime instead of hard-coding a device path
/// such as ACPI\PNP0C14\MIFS_0.
pub const WINDOWS_WMI_INSTANCE_QUERY: &str =
    "SELECT InstanceName, Active FROM MICommonInterface";

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn factory_wmi_schema_matches_acpi_wmi_mof_contract() {
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.namespace, r"ROOT\WMI");
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.class, "MICommonInterface");
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.method, "MiInterface");
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.input_property, "InData");
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.output_property, "OutData");
        assert_eq!(FACTORY_WINDOWS_WMI_SCHEMA.reserved_property, "Reserved");
    }

    #[test]
    fn instance_discovery_does_not_hardcode_mifs_device_path() {
        assert!(WINDOWS_WMI_INSTANCE_QUERY.contains("InstanceName"));
        assert!(WINDOWS_WMI_INSTANCE_QUERY.contains("MICommonInterface"));
    }
}
