//! Small conversions between Rust values and the Qt types QML sees.

use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QStringList, QVariant};

pub type QVariantMap = QMap<QMapPair_QString_QVariant>;
pub type QVariantList = QList<QVariant>;

pub fn qs(s: &str) -> QString {
    QString::from(s)
}

pub fn to_qstringlist<'a>(items: impl IntoIterator<Item = &'a String>) -> QStringList {
    items.into_iter().map(|s| QString::from(s)).collect()
}

pub fn qv_str(s: &str) -> QVariant {
    QVariant::from(&QString::from(s))
}

pub fn qv_strings(items: &[String]) -> QVariant {
    QVariant::from(&to_qstringlist(items))
}

/// A QVariantMap from (key, value) pairs.
pub fn qmap<const N: usize>(pairs: [(&str, QVariant); N]) -> QVariantMap {
    pairs.into_iter().map(|(k, v)| (QString::from(k), v)).collect()
}

/// QVariant::toString() of a value in an event's data.
pub fn variant_string(v: &QVariant) -> String {
    v.value_or_default::<QString>().to_string()
}

/// JSON (layout.json) to the QVariant tree QJsonDocument::toVariant() would give.
pub fn json_to_variant(v: &serde_json::Value) -> QVariant {
    use serde_json::Value;
    match v {
        Value::Null => QVariant::default(),
        Value::Bool(b) => QVariant::from(b),
        Value::Number(n) => match n.as_i64() {
            Some(i) => QVariant::from(&i),
            None => QVariant::from(&n.as_f64().unwrap_or(0.0)),
        },
        Value::String(s) => qv_str(s),
        Value::Array(a) => QVariant::from(&a.iter().map(json_to_variant).collect::<QVariantList>()),
        Value::Object(o) => QVariant::from(
            &o.iter().map(|(k, v)| (QString::from(k), json_to_variant(v))).collect::<QVariantMap>(),
        ),
    }
}
