use super::*;

impl SqliteDriver {
    pub(super) fn value_to_string(
        row: &sqlx::sqlite::SqliteRow,
        idx: usize,
        type_name: &str,
    ) -> Option<String> {
        let upper = type_name.to_ascii_uppercase();
        match upper.as_str() {
            "INTEGER" | "INT" | "INT8" | "BIGINT" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "REAL" | "FLOAT" | "DOUBLE" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BOOLEAN" => row
                .try_get::<Option<bool>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BLOB" => row
                .try_get::<Option<Vec<u8>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| format!("0x{}", hex::encode(v))),
            _ => row
                .try_get::<Option<String>, _>(idx)
                .ok()
                .flatten()
                .or_else(|| {
                    row.try_get::<Option<i64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                })
                .or_else(|| {
                    row.try_get::<Option<f64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                }),
        }
    }
}
