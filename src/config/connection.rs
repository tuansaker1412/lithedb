use directories::ProjectDirs;
use serde::{Deserialize, Serialize};
use std::fs;
use std::io;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum DriverType {
    PostgreSQL,
    MySQL,
    SQLite,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ConnectionConfig {
    pub id: String,
    pub name: String,
    pub driver: DriverType,
    pub host: String,
    pub port: u16,
    pub username: String,
    #[serde(skip)]
    pub password: String,
    pub database: String,
    pub ssl: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
struct ConnectionFile {
    connections: Vec<ConnectionConfig>,
}

pub struct ConnectionStore {
    path: PathBuf,
}

impl ConnectionStore {
    pub fn new() -> io::Result<Self> {
        let proj = ProjectDirs::from("org", "dbclient", "dbclient")
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "XDG config dir not found"))?;
        let dir = proj.config_dir();
        fs::create_dir_all(dir)?;
        Ok(Self {
            path: dir.join("connections.json"),
        })
    }

    pub fn from_path(path: PathBuf) -> Self {
        Self { path }
    }

    pub fn load(&self) -> io::Result<Vec<ConnectionConfig>> {
        if !self.path.exists() {
            return Ok(Vec::new());
        }

        let content = fs::read_to_string(&self.path)?;
        let data = serde_json::from_str::<ConnectionFile>(&content)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
        Ok(data.connections)
    }

    pub fn save(&self, connections: &[ConnectionConfig]) -> io::Result<()> {
        let parent = self
            .path
            .parent()
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "invalid config path"))?;
        fs::create_dir_all(parent)?;

        let data = ConnectionFile {
            connections: connections.to_vec(),
        };
        let json = serde_json::to_string_pretty(&data)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidData, e.to_string()))?;
        fs::write(&self.path, json)
    }

    pub fn delete_all(&self) -> io::Result<()> {
        if self.path.exists() {
            fs::remove_file(&self.path)?;
        }
        Ok(())
    }

    pub fn path(&self) -> &PathBuf {
        &self.path
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample(id: &str, password: &str) -> ConnectionConfig {
        ConnectionConfig {
            id: id.to_string(),
            name: format!("conn-{id}"),
            driver: DriverType::PostgreSQL,
            host: "localhost".to_string(),
            port: 5432,
            username: "user".to_string(),
            password: password.to_string(),
            database: "app".to_string(),
            ssl: false,
        }
    }

    #[test]
    fn save_load_round_trip_without_serializing_password() {
        let tempdir = std::env::temp_dir().join(format!("dbclient-test-{}", std::process::id()));
        let _ = fs::create_dir_all(&tempdir);
        let file = tempdir.join("connections.json");
        let store = ConnectionStore::from_path(file.clone());

        let input = vec![sample("a", "secret-a"), sample("b", "secret-b")];
        store.save(&input).expect("save failed");

        let raw = fs::read_to_string(file).expect("read failed");
        assert!(!raw.contains("secret-a"));
        assert!(!raw.contains("secret-b"));

        let loaded = store.load().expect("load failed");
        assert_eq!(loaded.len(), 2);
        assert_eq!(loaded[0].id, "a");
        assert_eq!(loaded[0].password, "");

        let _ = fs::remove_dir_all(&tempdir);
    }
}
