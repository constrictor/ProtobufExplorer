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

#include "Json.h"

#include <QCoreApplication>

#include <algorithm>

namespace protoexplorer {

namespace {

// Past this it is not data but a trap: every level takes some of the stack.
constexpr int kMaxDepth = 500;

QString tr(const char *text)
{
    return QCoreApplication::translate("Json", text);
}

bool isDigit(QChar c)
{
    return c >= u'0' && c <= u'9';
}

class Parser
{
public:
    explicit Parser(QStringView text) : m_text(text) {}

    bool parse(std::vector<JsonValue> &values)
    {
        if (m_text.startsWith(QChar(QChar::ByteOrderMark)))
            ++m_at;
        for (skipSpace(); m_at < m_text.size(); skipSpace()) {
            JsonValue value;
            value.keyBegin = m_at;
            if (!parseValue(value, 0))
                return false;
            values.push_back(std::move(value));
        }
        return true;
    }

    JsonError error() const
    {
        return {m_errorAt, int(m_text.first(m_errorAt).count(u'\n')) + 1, m_error};
    }

private:
    QChar peek() const { return m_at < m_text.size() ? m_text[m_at] : QChar(); }

    void skipSpace()
    {
        while (m_at < m_text.size()
               && (m_text[m_at] == u' ' || m_text[m_at] == u'\t' || m_text[m_at] == u'\n'
                   || m_text[m_at] == u'\r')) {
            ++m_at;
        }
    }

    bool fail(qsizetype at, const QString &text)
    {
        // Whatever was expected, what there is to say at the end is that it is the end.
        m_errorAt = std::min(at, m_text.size());
        m_error = at >= m_text.size() ? tr("the JSON ends too soon") : text;
        return false;
    }

    bool parseValue(JsonValue &value, int depth)
    {
        value.begin = m_at;
        bool ok = false;
        const QChar c = peek();
        if (c == u'{' || c == u'[')
            ok = parseMembers(value, depth);
        else if (c == u'"')
            ok = parseString(value.text, value.kind);
        else if (c == u'-' || isDigit(c))
            ok = parseNumber(value);
        else
            ok = parseWord(value);
        value.end = m_at;
        return ok;
    }

    bool parseMembers(JsonValue &value, int depth)
    {
        const bool object = peek() == u'{';
        const QChar close = object ? u'}' : u']';
        value.kind = object ? JsonValue::Kind::Object : JsonValue::Kind::Array;
        if (depth >= kMaxDepth)
            return fail(m_at, tr("nested too deeply"));
        ++m_at;
        skipSpace();
        if (peek() == close) {
            ++m_at;
            return true;
        }
        while (true) {
            JsonValue member;
            member.keyBegin = m_at;
            if (object) {
                if (peek() != u'"')
                    return fail(m_at, tr("expected a name in double quotes"));
                JsonValue::Kind kind;
                if (!parseString(member.key, kind))
                    return false;
                skipSpace();
                if (peek() != u':')
                    return fail(m_at, tr("expected ‘:’"));
                ++m_at;
                skipSpace();
            }
            if (!parseValue(member, depth + 1))
                return false;
            value.members.push_back(std::move(member));
            skipSpace();
            if (peek() == close) {
                ++m_at;
                return true;
            }
            if (peek() != u',')
                return fail(m_at, object ? tr("expected ‘,’ or ‘}’") : tr("expected ‘,’ or ‘]’"));
            ++m_at;
            skipSpace();
        }
    }

    bool parseString(QString &out, JsonValue::Kind &kind)
    {
        kind = JsonValue::Kind::String;
        const qsizetype open = m_at++;
        qsizetype from = m_at;
        while (true) {
            if (m_at >= m_text.size())
                return fail(open, tr("this string does not end"));
            const QChar c = m_text[m_at];
            if (c == u'"') {
                out += m_text.sliced(from, m_at - from);
                ++m_at;
                return true;
            }
            if (c == u'\n')
                return fail(open, tr("this string does not end"));
            if (c < u' ')
                return fail(m_at, tr("a control character, which has to be escaped"));
            if (c != u'\\') {
                ++m_at;
                continue;
            }
            out += m_text.sliced(from, m_at - from);
            if (!parseEscape(out))
                return false;
            from = m_at;
        }
    }

    bool parseEscape(QString &out)
    {
        const qsizetype begin = m_at++;
        const QChar c = peek();
        ++m_at;
        switch (c.unicode()) {
        case u'"':
        case u'\\':
        case u'/': out += c; return true;
        case u'b': out += u'\b'; return true;
        case u'f': out += u'\f'; return true;
        case u'n': out += u'\n'; return true;
        case u'r': out += u'\r'; return true;
        case u't': out += u'\t'; return true;
        case u'u': break;
        default:   return fail(begin, tr("not an escape that JSON has"));
        }
        char16_t unit;
        if (!parseHex(unit))
            return fail(begin, tr("expected four hexadecimal digits"));
        if (QChar::isLowSurrogate(unit))
            return fail(begin, tr("half of a character"));
        out += QChar(unit);
        if (!QChar::isHighSurrogate(unit))
            return true;
        if (!m_text.sliced(std::min(m_at, m_text.size())).startsWith(u"\\u"))
            return fail(begin, tr("half of a character"));
        m_at += 2;
        if (!parseHex(unit) || !QChar::isLowSurrogate(unit))
            return fail(begin, tr("half of a character"));
        out += QChar(unit);
        return true;
    }

    bool parseHex(char16_t &unit)
    {
        if (m_text.size() - m_at < 4)
            return false;
        bool ok = false;
        const QStringView digits = m_text.sliced(m_at, 4);
        // Not to let a sign or a space through.
        for (const QChar c : digits) {
            if (!isDigit(c) && !(c >= u'a' && c <= u'f') && !(c >= u'A' && c <= u'F'))
                return false;
        }
        unit = char16_t(digits.toUShort(&ok, 16));
        m_at += 4;
        return ok;
    }

    bool parseNumber(JsonValue &value)
    {
        const qsizetype begin = m_at;
        const auto digits = [this] {
            const qsizetype from = m_at;
            while (isDigit(peek()))
                ++m_at;
            return m_at > from;
        };
        if (peek() == u'-')
            ++m_at;
        if (peek() == u'0')
            ++m_at;
        else if (!digits())
            return fail(begin, tr("not a number"));
        if (isDigit(peek()))
            return fail(begin, tr("a number cannot begin with 0"));
        if (peek() == u'.') {
            ++m_at;
            if (!digits())
                return fail(begin, tr("not a number"));
        }
        if (peek() == u'e' || peek() == u'E') {
            ++m_at;
            if (peek() == u'+' || peek() == u'-')
                ++m_at;
            if (!digits())
                return fail(begin, tr("not a number"));
        }
        value.kind = JsonValue::Kind::Number;
        value.text = m_text.sliced(begin, m_at - begin).toString();
        return true;
    }

    bool parseWord(JsonValue &value)
    {
        const qsizetype begin = m_at;
        qsizetype end = begin;
        while (end < m_text.size() && (m_text[end].isLetterOrNumber() || m_text[end] == u'_'))
            ++end;
        const QStringView word = m_text.sliced(begin, end - begin);
        if (word == u"null")
            value.kind = JsonValue::Kind::Null;
        else if (word == u"true" || word == u"false")
            value.kind = JsonValue::Kind::Bool;
        else
            return fail(begin, tr("expected a value"));
        value.text = word.toString();
        m_at = end;
        return true;
    }

    QStringView m_text;
    qsizetype m_at = 0;
    qsizetype m_errorAt = 0;
    QString m_error;
};

} // namespace

const JsonValue *JsonValue::find(QStringView name) const
{
    const auto it = std::find_if(members.begin(), members.end(),
                                 [name](const JsonValue &member) { return member.key == name; });
    return kind == Kind::Object && it != members.end() ? &*it : nullptr;
}

JsonDocument parseJson(const QString &text)
{
    JsonDocument document;
    document.text = text;
    Parser parser(document.text);
    if (!parser.parse(document.values)) {
        document.values.clear();
        document.error = parser.error();
    }
    return document;
}

QString quoteJson(QStringView text)
{
    QString result;
    result.reserve(text.size() + 2);
    result += u'"';
    for (const QChar c : text) {
        switch (c.unicode()) {
        case u'"':  result += QStringLiteral("\\\""); break;
        case u'\\': result += QStringLiteral("\\\\"); break;
        case u'\b': result += QStringLiteral("\\b"); break;
        case u'\f': result += QStringLiteral("\\f"); break;
        case u'\n': result += QStringLiteral("\\n"); break;
        case u'\r': result += QStringLiteral("\\r"); break;
        case u'\t': result += QStringLiteral("\\t"); break;
        default:
            if (c < u' ' || c == u'\x7f')
                result += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, QLatin1Char('0'));
            else
                result += c;
        }
    }
    result += u'"';
    return result;
}

} // namespace protoexplorer
