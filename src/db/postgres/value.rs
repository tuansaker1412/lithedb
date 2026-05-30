use super::*;

impl PostgresDriver {
    pub(super) fn row_to_strings(row: &sqlx::postgres::PgRow) -> Vec<Option<String>> {
        row.columns()
            .iter()
            .enumerate()
            .map(|(idx, col)| Self::pg_value_to_string(row, idx, col.type_info().name()))
            .collect()
    }

    pub(super) fn pg_value_to_string(
        row: &sqlx::postgres::PgRow,
        idx: usize,
        type_name: &str,
    ) -> Option<String> {
        match type_name {
            "BOOL" => row
                .try_get::<Option<bool>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT2" => row
                .try_get::<Option<i16>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT4" => row
                .try_get::<Option<i32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INT8" => row
                .try_get::<Option<i64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT4" => row
                .try_get::<Option<f32>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "FLOAT8" => row
                .try_get::<Option<f64>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "BYTEA" => row
                .try_get::<Option<Vec<u8>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| format!("\\x{}", hex::encode(v))),
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
            "TIMESTAMP" => row
                .try_get::<Option<chrono::NaiveDateTime>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.format("%Y-%m-%d %H:%M:%S%.f").to_string()),
            "TIMESTAMPTZ" => row
                .try_get::<Option<chrono::DateTime<chrono::Utc>>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_rfc3339()),
            "TEXT" | "VARCHAR" | "BPCHAR" | "NAME" | "CHAR" | "CITEXT" | "UUID" | "INET"
            | "CIDR" | "MACADDR" | "MACADDR8" | "JSON" | "JSONB" | "XML" | "TIMETZ"
            | "INTERVAL" | "NUMERIC" | "MONEY" | "POINT" | "LINE" | "LSEG" | "BOX" | "PATH"
            | "POLYGON" | "CIRCLE" | "TSVECTOR" | "TSQUERY" => {
                row.try_get::<Option<String>, _>(idx).ok().flatten()
            }
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
