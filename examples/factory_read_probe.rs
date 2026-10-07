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

    match backend.read_performance_profile() {
        Ok(profile) => println!("performance profile: {profile:?}"),
        Err(error) => println!("performance profile read failed: {error:?}"),
    }

    match backend.read_charge_protection_80() {
        Ok(enabled) => println!("80% charge protection: {enabled}"),
        Err(error) => println!("80% charge protection read failed: {error:?}"),
    }

    match backend.read_microphone_mute_signal() {
        Ok(signal) => println!("microphone mute raw signal: {signal:?}"),
        Err(error) => println!("microphone mute raw signal read failed: {error:?}"),
    }
}

#[cfg(not(windows))]
fn main() {
    eprintln!("factory_read_probe is only available on Windows");
}
