use std::sync::OnceLock;

use serde::{Deserialize, Serialize};

use crate::config::paths;

pub const DEFAULT_MAX_QUERY_ROWS: u64 = 1000;
const MAX_QUERY_ROWS_ENV: &str = "LITHEDB_MAX_QUERY_ROWS";

static MAX_QUERY_ROWS: OnceLock<u64> = OnceLock::new();

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Settings {
    #[serde(default = "default_max_query_rows")]
    pub max_query_rows: u64,
}

fn default_max_query_rows() -> u64 {
    DEFAULT_MAX_QUERY_ROWS
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            max_query_rows: DEFAULT_MAX_QUERY_ROWS,
        }
    }
}

impl Settings {
    fn load_from_file() -> Settings {
        let Ok(path) = paths::prepare_config_file("settings.json") else {
            return Settings::default();
        };
        let Ok(content) = std::fs::read_to_string(&path) else {
            return Settings::default();
        };
        serde_json::from_str::<Settings>(&content).unwrap_or_default()
    }
}

fn resolve_max_query_rows() -> u64 {
    if let Ok(raw) = std::env::var(MAX_QUERY_ROWS_ENV) {
        if let Ok(value) = raw.trim().parse::<u64>() {
            if value > 0 {
                return value;
            }
        }
    }

    let from_file = Settings::load_from_file().max_query_rows;
    if from_file > 0 {
        from_file
    } else {
        DEFAULT_MAX_QUERY_ROWS
    }
}

pub fn max_query_rows() -> u64 {
    *MAX_QUERY_ROWS.get_or_init(resolve_max_query_rows)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn settings_defaults_when_field_missing() {
        let parsed: Settings = serde_json::from_str("{}").unwrap();
        assert_eq!(parsed.max_query_rows, DEFAULT_MAX_QUERY_ROWS);
    }

    #[test]
    fn settings_reads_explicit_value() {
        let parsed: Settings = serde_json::from_str(r#"{"max_query_rows": 5000}"#).unwrap();
        assert_eq!(parsed.max_query_rows, 5000);
    }
}
