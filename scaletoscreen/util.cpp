#include "util.h"
#include <QRegularExpression>

namespace scaleToScreen {

bool matchStringWithMode(const QString &text, const QString &pattern, MatchMode mode)
{
    switch (mode) {
    case MatchMode::Exact:
        return text == pattern;
    case MatchMode::Substring:
        return text.contains(pattern);
    case MatchMode::Regex: {
        const QRegularExpression re(pattern);
        return re.isValid() && re.match(text).hasMatch();
    }
    case MatchMode::Ignore:
    default:
        return true;
    }
}

} // namespace scaleToScreen
