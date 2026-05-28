use aes_gcm::aead::{Aead, KeyInit, Payload};
use aes_gcm::{Aes256Gcm, Nonce};
use rand::rngs::OsRng;
use rand::RngCore;
use sha2::{Digest, Sha256};
use std::fs;

const SERVICE: &str = "table-pro-linux";

fn machine_key_material() -> Option<Vec<u8>> {
    let machine_id = fs::read_to_string("/etc/machine-id").ok()?;
    Some(machine_id.trim().as_bytes().to_vec())
}

fn derive_key(connection_id: &str) -> Option<[u8; 32]> {
    let machine = machine_key_material()?;
    let mut hasher = Sha256::new();
    hasher.update(b"table-pro-linux-fallback-v1");
    hasher.update(machine);
    hasher.update(connection_id.as_bytes());
    let digest = hasher.finalize();
    let mut out = [0u8; 32];
    out.copy_from_slice(&digest);
    Some(out)
}

fn aad(connection_id: &str) -> Vec<u8> {
    format!("table-pro-linux:{connection_id}:v1").into_bytes()
}

fn fallback_encrypt_v1(connection_id: &str, plain: &str) -> Option<String> {
    let key = derive_key(connection_id)?;
    let cipher = Aes256Gcm::new_from_slice(&key).ok()?;

    let mut nonce_bytes = [0u8; 12];
    OsRng.fill_bytes(&mut nonce_bytes);
    let nonce = Nonce::from_slice(&nonce_bytes);

    let ciphertext = cipher
        .encrypt(
            nonce,
            Payload {
                msg: plain.as_bytes(),
                aad: &aad(connection_id),
            },
        )
        .ok()?;

    Some(format!(
        "enc:v1:{}:{}",
        hex::encode(nonce_bytes),
        hex::encode(ciphertext)
    ))
}

fn fallback_decrypt_v1(connection_id: &str, data: &str) -> Option<String> {
    let mut parts = data.split(':');
    let _enc = parts.next()?;
    let version = parts.next()?;
    let nonce_hex = parts.next()?;
    let cipher_hex = parts.next()?;
    if parts.next().is_some() || version != "v1" {
        return None;
    }

    let nonce_bytes = hex::decode(nonce_hex).ok()?;
    if nonce_bytes.len() != 12 {
        return None;
    }
    let cipher_bytes = hex::decode(cipher_hex).ok()?;

    let key = derive_key(connection_id)?;
    let cipher = Aes256Gcm::new_from_slice(&key).ok()?;
    let nonce = Nonce::from_slice(&nonce_bytes);
    let plaintext = cipher
        .decrypt(
            nonce,
            Payload {
                msg: cipher_bytes.as_ref(),
                aad: &aad(connection_id),
            },
        )
        .ok()?;

    String::from_utf8(plaintext).ok()
}

// Backward compatibility for old fallback format.
fn fallback_decrypt_legacy_enc01(connection_id: &str, data: &str) -> Option<String> {
    let raw = data.strip_prefix("enc:01")?;
    let bytes = hex::decode(raw).ok()?;
    let key = derive_key(connection_id)?;
    let cipher = Aes256Gcm::new_from_slice(&key).ok()?;
    let nonce = Nonce::from_slice(b"tableprolinuxnonce");
    let plaintext = cipher.decrypt(nonce, bytes.as_ref()).ok()?;
    String::from_utf8(plaintext).ok()
}

fn fallback_key(id: &str) -> String {
    format!("fallback-password-{id}")
}

pub fn store_password(id: &str, password: &str) -> Result<(), String> {
    let entry = keyring::Entry::new(SERVICE, id).map_err(|e| e.to_string())?;
    if entry.set_password(password).is_ok() {
        return Ok(());
    }

    let encrypted = fallback_encrypt_v1(id, password).ok_or("fallback encrypt failed")?;
    let fallback = keyring::Entry::new(SERVICE, &fallback_key(id)).map_err(|e| e.to_string())?;
    fallback.set_password(&encrypted).map_err(|e| e.to_string())
}

pub fn load_password(id: &str) -> Option<String> {
    let entry = keyring::Entry::new(SERVICE, id).ok()?;
    if let Ok(v) = entry.get_password() {
        return Some(v);
    }

    let fallback = keyring::Entry::new(SERVICE, &fallback_key(id)).ok()?;
    let encrypted = fallback.get_password().ok()?;
    fallback_decrypt_v1(id, &encrypted).or_else(|| fallback_decrypt_legacy_enc01(id, &encrypted))
}

pub fn delete_password(id: &str) -> Result<(), String> {
    if let Ok(entry) = keyring::Entry::new(SERVICE, id) {
        let _ = entry.delete_password();
    }
    if let Ok(fallback) = keyring::Entry::new(SERVICE, &fallback_key(id)) {
        let _ = fallback.delete_password();
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fallback_crypto_v1_round_trip() {
        let id = "conn-a";
        let plain = "super-secret";
        let encrypted = fallback_encrypt_v1(id, plain).expect("encrypt failed");
        assert!(encrypted.starts_with("enc:v1:"));
        let decrypted = fallback_decrypt_v1(id, &encrypted).expect("decrypt failed");
        assert_eq!(plain, decrypted);
    }

    #[test]
    fn fallback_crypto_rejects_tamper() {
        let id = "conn-b";
        let encrypted = fallback_encrypt_v1(id, "secret").expect("encrypt failed");
        let tampered = format!("{}0", encrypted);
        assert!(fallback_decrypt_v1(id, &tampered).is_none());
    }

    #[test]
    fn fallback_crypto_binds_to_connection_id() {
        let encrypted = fallback_encrypt_v1("conn-c", "secret").expect("encrypt failed");
        assert!(fallback_decrypt_v1("conn-other", &encrypted).is_none());
    }

    #[test]
    fn legacy_enc01_is_still_supported() {
        let id = "conn-legacy";
        let key = derive_key(id).expect("key");
        let cipher = Aes256Gcm::new_from_slice(&key).expect("cipher");
        let nonce = Nonce::from_slice(b"tableprolinuxnonce");
        let ciphertext = cipher.encrypt(nonce, b"legacy-secret".as_ref()).expect("enc");
        let legacy = format!("enc:01{}", hex::encode(ciphertext));
        let decrypted = fallback_decrypt_legacy_enc01(id, &legacy).expect("dec");
        assert_eq!(decrypted, "legacy-secret");
    }
}
