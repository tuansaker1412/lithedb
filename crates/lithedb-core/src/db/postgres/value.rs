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
            "UUID" => row
                .try_get::<Option<sqlx::types::Uuid>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "NUMERIC" => row
                .try_get::<Option<sqlx::types::BigDecimal>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "JSON" | "JSONB" => row
                .try_get::<Option<sqlx::types::JsonValue>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "INET" | "CIDR" => row
                .try_get::<Option<sqlx::types::ipnetwork::IpNetwork>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MACADDR" => row
                .try_get::<Option<sqlx::types::mac_address::MacAddress>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.to_string()),
            "MONEY" => row
                .try_get::<Option<sqlx::postgres::types::PgMoney>, _>(idx)
                .ok()
                .flatten()
                .map(|v| v.0.to_string()),
            "INTERVAL" => row
                .try_get::<Option<sqlx::postgres::types::PgInterval>, _>(idx)
                .ok()
                .flatten()
                .map(Self::pg_interval_to_string),
            "TIMETZ" => row
                .try_get::<
                    Option<sqlx::postgres::types::PgTimeTz<chrono::NaiveTime, chrono::FixedOffset>>,
                    _,
                >(idx)
                .ok()
                .flatten()
                .map(|v| format!("{}{}", v.time, v.offset)),
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
            "TEXT" | "VARCHAR" | "BPCHAR" | "NAME" | "CHAR" | "CITEXT" => {
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
                })
                .or_else(|| {
                    row.try_get::<Option<f64>, _>(idx)
                        .ok()
                        .flatten()
                        .map(|v| v.to_string())
                }),
        }
    }

    fn pg_interval_to_string(interval: sqlx::postgres::types::PgInterval) -> String {
        let years = interval.months / 12;
        let mons = interval.months % 12;
        let total_seconds = interval.microseconds / 1_000_000;
        let hours = total_seconds / 3600;
        let minutes = (total_seconds % 3600) / 60;
        let seconds = total_seconds % 60;
        let micros = interval.microseconds % 1_000_000;

        let mut parts: Vec<String> = Vec::new();
        if years != 0 {
            parts.push(format!("{} years", years));
        }
        if mons != 0 {
            parts.push(format!("{} mons", mons));
        }
        if interval.days != 0 {
            parts.push(format!("{} days", interval.days));
        }
        let has_time =
            hours != 0 || minutes != 0 || seconds != 0 || micros != 0 || parts.is_empty();
        if has_time {
            if micros != 0 {
                parts.push(format!(
                    "{:02}:{:02}:{:02}.{:06}",
                    hours, minutes, seconds, micros
                ));
            } else {
                parts.push(format!("{:02}:{:02}:{:02}", hours, minutes, seconds));
            }
        }
        parts.join(" ")
    }
}
