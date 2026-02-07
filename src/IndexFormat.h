#pragma once
#include <QByteArray>

inline QByteArray escapeTSV(const QByteArray& in) {
    QByteArray out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += c; break;
        }
    }
    return out;
}

inline QByteArray unescapeTSV(const QByteArray& in) {
    QByteArray out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); i++) {
        char c = in[i];
        if (c == '\\' && i + 1 < in.size()) {
            char n = in[i + 1];
            if (n == '\\') { out += '\\'; i++; continue; }
            if (n == 't')  { out += '\t'; i++; continue; }
            if (n == 'n')  { out += '\n'; i++; continue; }
            if (n == 'r')  { out += '\r'; i++; continue; }
        }
        out += c;
    }
    return out;
}
