use super::*;

impl MySqlDriver {
    pub(super) fn row_to_strings(row: &sqlx::mysql::MySqlRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::value_to_string(row, idx, col.type_info().name()))
            .collect()
    }

    pub(super) fn value_to_string(
        row: &sqlx::mysql::MySqlRow,
        idx: usize,
        type_name: &str,
    ) -> Option<String> {
        let upper = type_name.to_ascii_uppercase();
        match upper.as_str() {
            "BOOLEAN" | "BOOL" | "TINYINT(1)" | "TINYINT" => row
                .try_get::<Option<i8>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "SMALLINT" => row
                .try_get::<Option<i16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MEDIUMINT" | "INT" | "INTEGER" => row
                .try_get::<Option<i32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BIGINT" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "TINYINT UNSIGNED" => row
                .try_get::<Option<u8>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "SMALLINT UNSIGNED" => row
                .try_get::<Option<u16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MEDIUMINT UNSIGNED" | "INT UNSIGNED" => row
                .try_get::<Option<u32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BIGINT UNSIGNED" => row
                .try_get::<Option<u64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT" => row
                .try_get::<Option<f32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "DOUBLE" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BLOB" | "TINYBLOB" | "MEDIUMBLOB" | "LONGBLOB" | "VARBINARY" | "BINARY" => row
                .try_get::<Option<Vec<u8>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| format!("0x{}", hex::encode(v))),
            "DATE" => row
                .try_get::<Option<chrono::NaiveDate>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "TIME" => row
                .try_get::<Option<chrono::NaiveTime>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "DATETIME" | "TIMESTAMP" => row
                .try_get::<Option<chrono::NaiveDateTime>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.format("%Y-%m-%d %H:%M:%S%.f").to_string()),
            _ => row
                .try_get::<Option<String>, _>(idx)
                .ok()
                .flatten()
                .or_else(|| {
                    row.try_get::<Option<i64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                }),
        }
    }
}
