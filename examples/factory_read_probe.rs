#[cfg(windows)]
fn main() {
    use redmibook_manager_core::{FactoryFirmwareBackend, WindowsMifsWmiTransport};

    let transport = match WindowsMifsWmiTransport::connect() {
        Ok(transport) => transport,
        Err(error) => {
            eprintln!("factory WMI discovery failed: {error:?}");
            std::process::exit(1);
        }
    };

    println!("factory WMI instance: {}", transport.instance_name());

    let backend = FactoryFirmwareBackend::new(transport);
    let mut read_failures = 0u8;

    match backend.read_performance_profile() {
        Ok(profile) => println!("performance profile: {profile:?}"),
        Err(error) => {
            eprintln!("performance profile read failed: {error:?}");
            read_failures += 1;
        },
    }

    match backend.read_charge_protection_80() {
        Ok(enabled) => println!("80% charge protection: {enabled}"),
        Err(error) => {
            eprintln!("80% charge protection read failed: {error:?}");
            read_failures += 1;
        },
    }

    match backend.read_microphone_mute_signal() {
        Ok(signal) => println!("microphone mute raw signal: {signal:?}"),
        Err(error) => {
            eprintln!("microphone mute raw signal read failed: {error:?}");
            read_failures += 1;
        },
    }

    if read_failures != 0 {
        eprintln!("Read-only validation incomplete: {read_failures} of 3 reads failed");
        std::process::exit(2);
    }

    println!("Read-only validation: all 3 reads succeeded");
}

#[cfg(not(windows))]
fn main() {
    eprintln!("factory_read_probe is only available on Windows");
}
