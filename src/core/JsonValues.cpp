// protobufexplorer — a visual explorer for Protocol Buffers schemas.
// Copyright (C) 2026 Valentyn Pavliuchenko
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "JsonValues.h"

#include <QCoreApplication>
#include <QDate>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace protoexplorer {

namespace {

constexpr qsizetype kMaxShown = 120;
constexpr qsizetype kMaxBytesShown = 16;

QString tr(const char *text)
{
    return QCoreApplication::translate("JsonValues", text);
}

QString outOfRange(QStringView scalar)
{
    return tr("out of range for %1").arg(scalar);
}

bool isInteger(QStringView scalar)
{
    return scalar.endsWith(u"32") || scalar.endsWith(u"64");
}

bool is64Bit(QStringView scalar)
{
    return scalar.endsWith(u"64");
}

bool isUnsigned(QStringView scalar)
{
    return scalar.startsWith(u'u') || scalar.startsWith(u"fixed");
}

bool isPlainInteger(const QString &text)
{
    static const QRegularExpression integer(QStringLiteral("\\A-?[0-9]+\\z"));
    return integer.match(text).hasMatch();
}

bool fits(QStringView scalar, const QString &digits)
{
    bool ok = false;
    if (isUnsigned(scalar)) {
        const qulonglong n = digits.toULongLong(&ok);
        return ok && (is64Bit(scalar) || n <= std::numeric_limits<quint32>::max());
    }
    const qlonglong n = digits.toLongLong(&ok);
    return ok
        && (is64Bit(scalar)
            || (n >= std::numeric_limits<qint32>::min() && n <= std::numeric_limits<qint32>::max()));
}

// Whole numbers can be written with a point or an exponent too, and to the last digit: a
// double does not hold every one of them.
Reading readInteger(QStringView scalar, const JsonValue &json)
{
    if (json.kind != JsonValue::Kind::Number && json.kind != JsonValue::Kind::String)
        return {{}, tr("expected a number")};
    static const QRegularExpression number(
        QStringLiteral("\\A(-?)([0-9]+)(?:\\.([0-9]+))?(?:[eE]([+-]?[0-9]+))?\\z"));
    const auto match = number.match(json.text);
    if (!match.hasMatch())
        return {{}, tr("not a number")};

    QString digits = match.captured(2) + match.captured(3);
    qsizetype shift = -match.captured(3).size();
    while (digits.endsWith(u'0')) {
        digits.chop(1);
        ++shift;
    }
    qsizetype zeros = 0;
    while (zeros < digits.size() && digits[zeros] == u'0')
        ++zeros;
    digits.remove(0, zeros);
    if (digits.isEmpty())
        return {QStringLiteral("0"), {}};

    bool ok = true;
    const QString exponent = match.captured(4);
    const int power = exponent.isEmpty() ? 0 : exponent.toInt(&ok);
    if (!ok)
        return {{}, exponent.startsWith(u'-') ? tr("not a whole number") : outOfRange(scalar)};
    const qint64 places = qint64(shift) + power;
    if (places < 0)
        return {{}, tr("not a whole number")};
    if (places > 20)
        return {{}, outOfRange(scalar)};
    digits += QString(qsizetype(places), u'0');
    digits.prepend(match.captured(1));
    return fits(scalar, digits) ? Reading{digits, {}} : Reading{{}, outOfRange(scalar)};
}

Reading readFloat(QStringView scalar, const JsonValue &json)
{
    if (json.kind == JsonValue::Kind::String
        && (json.text == u"NaN" || json.text == u"Infinity" || json.text == u"-Infinity")) {
        return {json.text, {}};
    }
    if (json.kind != JsonValue::Kind::Number && json.kind != JsonValue::Kind::String)
        return {{}, tr("expected a number")};
    static const QRegularExpression number(
        QStringLiteral("\\A-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?\\z"));
    if (!number.match(json.text).hasMatch())
        return {{}, tr("not a number")};
    // Zero when it is too small to tell from it, which is no reason to turn it down.
    const double value = std::abs(json.text.toDouble());
    // The first double that does not round down to the largest float.
    constexpr double tooBig = 0x1.ffffffp127;
    if (std::isinf(value) || (scalar == u"float" && value >= tooBig))
        return {{}, outOfRange(scalar)};
    return {json.text, {}};
}

// Either alphabet, with or without the padding.
std::optional<QByteArray> decodeBase64(QString text)
{
    if (text.endsWith(u'=')) {
        if (text.size() % 4 != 0)
            return {};
        text.chop(text.endsWith(u"==") ? 2 : 1);
    }
    text.replace(u'-', u'+').replace(u'_', u'/');
    static const QRegularExpression alphabet(QStringLiteral("\\A[A-Za-z0-9+/]*\\z"));
    if (text.size() % 4 == 1 || !alphabet.match(text).hasMatch())
        return {};
    return QByteArray::fromBase64(text.toLatin1(), QByteArray::OmitTrailingEquals);
}

QString quoted(QStringView text)
{
    const bool cut = text.size() > kMaxShown;
    QString result = quoteJson(cut ? text.first(kMaxShown) : text);
    if (cut)
        result.insert(result.size() - 1, u'…');
    return result;
}

// What they spell if they spell something, and otherwise what they are.
QString describeBytes(const QByteArray &bytes)
{
    const QString size = bytes.size() == 1 ? tr("1 byte") : tr("%1 bytes").arg(bytes.size());
    if (bytes.isEmpty())
        return size;
    if (bytes.isValidUtf8()) {
        const QString text = QString::fromUtf8(bytes);
        if (std::all_of(text.begin(), text.end(), [](QChar c) { return c.isPrint() || c.isSurrogate(); }))
            return size + u' ' + quoted(text);
    }
    QString hex = QString::fromLatin1(bytes.first(std::min(bytes.size(), kMaxBytesShown)).toHex(' '));
    if (bytes.size() > kMaxBytesShown)
        hex += QStringLiteral(" …");
    return size + u' ' + hex;
}

const Decl *valueNamed(const Decl &type, QStringView name)
{
    for (const auto &value : type.members) {
        if (value->name == name)
            return value.get();
    }
    return nullptr;
}

const Decl *valueNumbered(const Decl &type, int number)
{
    for (const auto &value : type.members) {
        if (value->number == number)
            return value.get();
    }
    return nullptr;
}

} // namespace

Special specialOf(const Decl &type)
{
    static const QStringView package = u"google.protobuf.";
    if (!type.fullName.startsWith(package))
        return Special::None;
    const QStringView name = QStringView(type.fullName).sliced(package.size());
    if (type.kind == DeclKind::Enum)
        return name == u"NullValue" ? Special::NullValue : Special::None;
    static const std::pair<QStringView, Special> names[] = {
        {u"Any", Special::Any},
        {u"Value", Special::Value},
        {u"Struct", Special::Struct},
        {u"ListValue", Special::ListValue},
        {u"Timestamp", Special::Timestamp},
        {u"Duration", Special::Duration},
        {u"FieldMask", Special::FieldMask},
    };
    for (const auto &[known, special] : names) {
        if (name == known)
            return special;
    }
    return wrappedScalar(type).isEmpty() ? Special::None : Special::Wrapper;
}

QString wrappedScalar(const Decl &wrapper)
{
    static const QStringList scalars{
        QStringLiteral("double"), QStringLiteral("float"), QStringLiteral("int64"),
        QStringLiteral("uint64"), QStringLiteral("int32"), QStringLiteral("uint32"),
        QStringLiteral("bool"), QStringLiteral("string"), QStringLiteral("bytes")};
    if (!wrapper.fullName.startsWith(u"google.protobuf.") || !wrapper.name.endsWith(u"Value"))
        return {};
    const QString wrapped = wrapper.name.chopped(5).toLower();
    const auto it = std::find(scalars.begin(), scalars.end(), wrapped);
    return it == scalars.end() ? QString() : *it;
}

Reading readScalar(QStringView scalar, const JsonValue &json)
{
    using Kind = JsonValue::Kind;
    if (isInteger(scalar))
        return readInteger(scalar, json);
    if (scalar == u"double" || scalar == u"float")
        return readFloat(scalar, json);
    if (scalar == u"bool")
        return json.kind == Kind::Bool ? Reading{json.text, {}} : Reading{{}, tr("expected true or false")};
    if (json.kind != Kind::String)
        return {{}, tr("expected a string")};
    if (scalar == u"string")
        return {quoted(json.text), {}};
    const auto bytes = decodeBase64(json.text);
    return bytes ? Reading{describeBytes(*bytes), {}} : Reading{{}, tr("not base64")};
}

Reading readEnum(const Decl &type, const JsonValue &json)
{
    using Kind = JsonValue::Kind;
    if (specialOf(type) == Special::NullValue && json.kind == Kind::Null)
        return {QStringLiteral("NULL_VALUE"), {}};
    if (json.kind != Kind::String && json.kind != Kind::Number)
        return {{}, tr("expected a name or a number")};
    if (json.kind == Kind::String) {
        if (valueNamed(type, json.text))
            return {json.text, {}};
        if (!isPlainInteger(json.text))
            return {{}, tr("no such value in %1").arg(type.name)};
    }
    const Reading number = readInteger(u"int32", json);
    if (!number.problem.isEmpty())
        return number;
    const Decl *value = valueNumbered(type, number.text.toInt());
    if (!value && type.closed)
        return {{}, tr("no such value in %1").arg(type.name)};
    return {value ? QStringLiteral("%1 (%2)").arg(value->name, number.text)
                  : tr("%1 (no name)").arg(number.text), {}};
}

Reading readSpecial(Special special, const JsonValue &json)
{
    if (json.kind != JsonValue::Kind::String)
        return {{}, tr("expected a string")};
    if (special == Special::Timestamp) {
        static const QRegularExpression time(QStringLiteral(
            "\\A([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2})(?:\\.[0-9]{1,9})?"
            "(?:Z|[+-]([0-9]{2}):([0-9]{2}))\\z"));
        const auto m = time.match(json.text);
        const auto n = [&m](int group) { return m.captured(group).toInt(); };
        if (!m.hasMatch() || !QDate::isValid(n(1), n(2), n(3)) || n(1) == 0 || n(4) > 23
            || n(5) > 59 || n(6) > 59 || n(7) > 23 || n(8) > 59) {
            return {{}, tr("not a timestamp")};
        }
    } else if (special == Special::Duration) {
        static const QRegularExpression span(
            QStringLiteral("\\A(-?[0-9]{1,12})(?:\\.[0-9]{1,9})?s\\z"));
        const auto m = span.match(json.text);
        if (!m.hasMatch())
            return {{}, tr("not a duration")};
        if (std::abs(m.captured(1).toLongLong()) > 315'576'000'000)
            return {{}, tr("out of range for a duration")};
    } else if (special == Special::FieldMask) {
        static const QRegularExpression mask(QStringLiteral("\\A[A-Za-z0-9.,]*\\z"));
        if (!mask.match(json.text).hasMatch())
            return {{}, tr("not a field mask")};
        return {quoted(json.text), {}};
    }
    return {json.text, {}};
}

QString mapKeyProblem(QStringView scalar, const QString &key)
{
    if (scalar == u"bool")
        return key == u"true" || key == u"false" ? QString() : tr("the key is not true or false");
    if (!isInteger(scalar))
        return {};
    if (!isPlainInteger(key))
        return tr("the key is not a number");
    return fits(scalar, key) ? QString() : tr("the key is out of range for %1").arg(scalar);
}

QString defaultText(const Decl &field)
{
    const FieldInfo &info = *field.field;
    if (info.repeated)
        return tr("0 items");
    if (!info.mapKey.isEmpty())
        return tr("0 entries");
    if (field.type && field.type->kind == DeclKind::Enum) {
        if (info.hasDefault)
            return QString::fromUtf8(info.defaultValue);
        return field.type->members.empty() ? QString() : field.type->members.front()->name;
    }
    const QString &scalar = info.scalar;
    if (scalar.isEmpty())
        return {};
    if (scalar == u"string")
        return quoted(QString::fromUtf8(info.defaultValue));
    if (scalar == u"bytes")
        return describeBytes(info.defaultValue);
    if (info.hasDefault)
        return QString::fromUtf8(info.defaultValue);
    return scalar == u"bool" ? QStringLiteral("false") : QStringLiteral("0");
}

QString defaultJson(const Decl &field)
{
    const FieldInfo &info = *field.field;
    if (info.repeated)
        return QStringLiteral("[]");
    if (!info.mapKey.isEmpty())
        return QStringLiteral("{}");
    if (field.type && field.type->kind == DeclKind::Enum) {
        return specialOf(*field.type) == Special::NullValue ? QStringLiteral("null")
                                                            : quoteJson(defaultText(field));
    }
    const QString &scalar = info.scalar;
    if (scalar.isEmpty())
        return QStringLiteral("null");
    if (scalar == u"string" || scalar == u"bytes")
        return QStringLiteral("\"\"");
    if (scalar == u"bool")
        return QStringLiteral("false");
    return is64Bit(scalar) ? QStringLiteral("\"0\"") : QStringLiteral("0");
}

QString elided(QStringView text)
{
    QString line = text.first(std::min(text.size(), 4 * kMaxShown)).toString().simplified();
    if (line.size() > kMaxShown || text.size() > 4 * kMaxShown) {
        line.truncate(kMaxShown);
        line += u'…';
    }
    return line;
}

} // namespace protoexplorer
