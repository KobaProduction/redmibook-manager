use crate::backend::CoolingTelemetryBackend;
use crate::capability::CpuFanSpeedRpm;

/// Narrow factory-telemetry transport for the confirmed TM2309 PTID RPM value.
///
/// Implementations may use an existing Intel PTID provider or a scoped Windows
/// device-stack helper. They must expose this named telemetry operation rather
/// than a generic arbitrary ACPI-evaluation surface to the application.
pub trait FactoryPtIdTransport {
    type Error;

    fn read_cpu_fan1_speed_rpm(&self) -> Result<u64, Self::Error>;
}

impl<F, E> FactoryPtIdTransport for F
where
    F: Fn() -> Result<u64, E>,
{
    type Error = E;

    fn read_cpu_fan1_speed_rpm(&self) -> Result<u64, Self::Error> {
        (self)()
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum FactoryTelemetryError<E> {
    Transport(E),
    ValueOutOfRange(u64),
}

pub struct FactoryCoolingTelemetryBackend<T> {
    transport: T,
}

impl<T> FactoryCoolingTelemetryBackend<T>
where
    T: FactoryPtIdTransport,
{
    pub fn new(transport: T) -> Self {
        Self { transport }
    }

    pub fn read_cpu_fan1_speed_rpm(
        &self,
    ) -> Result<CpuFanSpeedRpm, FactoryTelemetryError<T::Error>> {
        let raw = self
            .transport
            .read_cpu_fan1_speed_rpm()
            .map_err(FactoryTelemetryError::Transport)?;
        let rpm = u32::try_from(raw).map_err(|_| FactoryTelemetryError::ValueOutOfRange(raw))?;
        Ok(CpuFanSpeedRpm::new(rpm))
    }
}

impl<T> CoolingTelemetryBackend for FactoryCoolingTelemetryBackend<T>
where
    T: FactoryPtIdTransport,
{
    type Error = FactoryTelemetryError<T::Error>;

    fn read_cpu_fan1_speed_rpm(&self) -> Result<CpuFanSpeedRpm, Self::Error> {
        FactoryCoolingTelemetryBackend::read_cpu_fan1_speed_rpm(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn returns_semantic_cpu_fan_rpm() {
        let backend = FactoryCoolingTelemetryBackend::new(|| -> Result<u64, ()> { Ok(3125) });
        assert_eq!(
            backend.read_cpu_fan1_speed_rpm(),
            Ok(CpuFanSpeedRpm::new(3125))
        );
    }

    #[test]
    fn rejects_values_that_do_not_fit_semantic_rpm_type() {
        let raw = u64::from(u32::MAX) + 1;
        let backend = FactoryCoolingTelemetryBackend::new(move || -> Result<u64, ()> { Ok(raw) });
        assert_eq!(
            backend.read_cpu_fan1_speed_rpm(),
            Err(FactoryTelemetryError::ValueOutOfRange(raw))
        );
    }
}
