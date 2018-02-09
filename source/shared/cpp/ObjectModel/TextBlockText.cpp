#if defined(__ANDROID__) || (__APPLE__) || (__linux__)
#define LOCALTIME(X,Y) (nullptr == localtime_r(Y, X))
#else
#define LOCALTIME(X,Y) localtime_s(X,Y)
#endif

#include "TextBlockText.h"

#include "pch.h"
#include "BaseCardElement.h"
#include "Enums.h"
#include <time.h>
#include "ElementParserRegistration.h"
#include "TextBlockText.h"
#include <iomanip>
#include <regex>
#include <iostream>
#include <codecvt>

using namespace AdaptiveCards;

TextSection::TextSection() : m_text(""), m_format(TextSectionFormat::RegularString)
{
}

TextSection::TextSection(std::string text, TextSectionFormat format) : m_text(text), m_format(format)
{
}

std::string TextSection::GetText() const
{
    return m_text;
}

std::string TextSection::GetOriginalText() const
{
    return m_originalText;
}

TextSectionFormat TextSection::GetFormat() const
{
    return m_format;
}

int TextSection::GetDay() const
{
    int day{};
    if (m_format == TextSectionFormat::DateShort || 
        m_format == TextSectionFormat::DateLong || 
        m_format == TextSectionFormat::DateCompact)
    {
        day = std::stoi(Split(m_text, '/').at(1));
    }
    return day;
}

int TextSection::GetMonth() const
{
    int minute{};
    if (m_format == TextSectionFormat::DateShort ||
        m_format == TextSectionFormat::DateLong ||
        m_format == TextSectionFormat::DateCompact)
    {
        minute = std::stoi(Split(m_text, '/').at(0));
    }
    return minute;
}

int TextSection::GetYear() const
{
    int year{};
    if (m_format == TextSectionFormat::DateShort ||
        m_format == TextSectionFormat::DateLong ||
        m_format == TextSectionFormat::DateCompact)
    {
        year = std::stoi(Split(m_text, '/').at(2));
    }
    return year;
}

int TextSection::GetSecond() const
{
    int second{};
    if (m_format == TextSectionFormat::Time)
    {
        second = std::stoi(Split(m_text, ':').at(2));
    }
    return second;
}

int TextSection::GetMinute() const
{
    int minute{};
    if (m_format == TextSectionFormat::Time)
    {
        minute = std::stoi(Split(m_text, ':').at(1));
    }
    return minute;
}

int TextSection::GetHour() const
{
    int hour{};
    if (m_format == TextSectionFormat::Time)
    {
        hour = std::stoi(Split(m_text, ':').at(0));
    }
    return hour;
}

std::vector<std::string> TextSection::Split(const std::string& in, char delimiter) const
{
    std::vector<std::string> tokens;
    size_t startIndex{};
    size_t endIndex = in.find(delimiter, startIndex);

    while (std::string::npos != endIndex)
    {
        std::string token = in.substr(startIndex, endIndex - startIndex);
        tokens.emplace_back(token);
        startIndex = endIndex + 1;
        endIndex = in.find(delimiter, startIndex);
    }

    tokens.emplace_back(in.substr(startIndex));
    return tokens;
}

TextBlockText::TextBlockText()
{
}

TextBlockText::TextBlockText(std::string in)
{
    ParseDateTime(in);
}

std::vector<TextSection> TextBlockText::GetString() const
{
    return m_fullString;
}

void TextBlockText::AddTextSection(std::string text, TextSectionFormat format)
{
    m_fullString.emplace_back(TextSection(text, format));
}

std::string TextBlockText::Concatenate()
{
    std::string formedString;
    for (const auto& piece : m_fullString)
    {
        formedString += piece.GetOriginalText();
    }
    return formedString;
}

bool TextBlockText::IsValidTimeAndDate(const struct tm &parsedTm, int hours, int minutes)
{
    if (parsedTm.tm_mon <= 12 && parsedTm.tm_mday <= 31 && parsedTm.tm_hour <= 24 &&
        parsedTm.tm_min <= 60 && parsedTm.tm_sec <= 60 && hours <= 24 && minutes <= 60)
    {
        if (parsedTm.tm_mon == 4 || parsedTm.tm_mon == 6 || parsedTm.tm_mon == 9 || parsedTm.tm_mon == 11)
        {
            return parsedTm.tm_mday <= 30;
        }
        else if (parsedTm.tm_mon == 2)
        {
            /// check for leap year
            if ((parsedTm.tm_year % 4 == 0 && parsedTm.tm_year % 100 != 0) || parsedTm.tm_year % 400 == 0)
            {
                return parsedTm.tm_mday <= 29;
            }

            return parsedTm.tm_mday <= 28;
        }

        return true;
    }
    return false;
}

void TextBlockText::ParseDateTime(std::string in)
{
    std::vector<TextSection> sections;

    std::wregex pattern(L"\\{\\{((DATE)|(TIME))\\((\\d{4})-{1}(\\d{2})-{1}(\\d{2})T(\\d{2}):{1}(\\d{2}):{1}(\\d{2})(Z|(([+-])(\\d{2}):{1}(\\d{2})))((((, ?SHORT)|(, ?LONG))|(, ?COMPACT))|)\\)\\}\\}");
    std::wsmatch matches;
    std::wstring text = StringToWstring(in);
    enum MatchIndex
    {
        IsDate = 2,
        Year = 4,
        Month,
        Day,
        Hour,
        Min,
        Sec,
        TimeZone = 12,
        TZHr,
        TZMn,
        Format,
        Style,
    };
    std::vector<int> indexer = {Year, Month, Day, Hour, Min, Sec, TZHr, TZMn};

    while (std::regex_search(text, matches, pattern))
    {
        time_t offset{};
        int  formatStyle{};
        // Date is matched
        bool isDate = matches[IsDate].matched;
        int hours{}, minutes{};
        struct tm parsedTm{};
        int *addrs[] = {&parsedTm.tm_year, &parsedTm.tm_mon,
            &parsedTm.tm_mday, &parsedTm.tm_hour, &parsedTm.tm_min,
            &parsedTm.tm_sec, &hours, &minutes};

        if (matches[Style].matched)
        {
            // match for long/short/compact
            bool formatHasSpace = matches[Format].str()[1] == ' ';
            int formatStartIndex = formatHasSpace ? 2 : 1;
            formatStyle = matches[Format].str()[formatStartIndex];
        }

        AddTextSection(WstringToString(matches.prefix().str()), TextSectionFormat::RegularString);

        if (!isDate && formatStyle)
        {
            AddTextSection(WstringToString(matches[0].str()), TextSectionFormat::RegularString);
            text = matches.suffix().str();
            continue;
        }

        for (unsigned int idx = 0; idx < indexer.size(); idx++)
        {
            if (matches[indexer[idx]].matched)
            {
                // get indexes for time attributes to index into conrresponding matches
                // and covert it to string
                *addrs[idx] = stoi(matches[indexer[idx]]);
            }
        }

        // check for date and time validation
        if (IsValidTimeAndDate(parsedTm, hours, minutes))
        {
            // maches offset sign, 
            // Z == UTC, 
            // + == time added from UTC
            // - == time subtracted from UTC
            if (matches[TimeZone].matched)
            {
                // converts to seconds
                hours *= 3600;
                minutes *= 60;
                offset = hours + minutes;

                wchar_t zone = matches[TimeZone].str()[0];
                // time zone offset calculation 
                if (zone == '+')
                {
                    offset *= -1;
                }
            }

            // measured from year 1900
            parsedTm.tm_year -= 1900;
            parsedTm.tm_mon -= 1;

            time_t utc{};
            // converts to ticks in UTC
            utc = mktime(&parsedTm);
            if (utc == -1)
            {
                AddTextSection(WstringToString(matches[0]), TextSectionFormat::RegularString);
            }

            wchar_t tzOffsetBuff[6]{};
            // gets local time zone offset
            wcsftime(tzOffsetBuff, 6, L"%z", &parsedTm);
            std::wstring localTimeZoneOffsetStr(tzOffsetBuff);
            int nTzOffset = std::stoi(localTimeZoneOffsetStr);
            offset += ((nTzOffset / 100) * 3600 + (nTzOffset % 100) * 60);
            // add offset to utc
            utc += offset;
            struct tm result{};

            // converts to local time from utc
            if (!LOCALTIME(&result, &utc))
            {
                // localtime() set dst, put_time adjusts time accordingly which is not what we want since 
                // we have already taken cared of it in our calculation
                if (result.tm_isdst == 1)
                {
                    result.tm_hour -= 1;
                }

                if (isDate)
                {
                    std::string plainDate = std::to_string(result.tm_mon) + "/" +
                        std::to_string(result.tm_mday) + "/" +
                        std::to_string(result.tm_year + 1900);

                    switch (formatStyle)
                    {
                        // SHORT Style
                        case 'S':
                            AddTextSection(plainDate, TextSectionFormat::DateShort);
                            break;
                        // LONG Style
                        case 'L':
                            AddTextSection(plainDate, TextSectionFormat::DateLong);
                            break;
                        // COMPACT or DEFAULT Style
                        case 'C': default:
                            AddTextSection(plainDate, TextSectionFormat::DateCompact);
                            break;
                    }
                }
                else
                {
                    AddTextSection(std::to_string(result.tm_hour) + ":" +
                        std::to_string(result.tm_min) + ":" +
                        std::to_string(result.tm_sec),
                        TextSectionFormat::Time);
                }
            }
        }
        else
        {
            AddTextSection(WstringToString(matches[0].str()), TextSectionFormat::RegularString);
        }
        
        text = matches.suffix().str();
    }

    AddTextSection(WstringToString(text), TextSectionFormat::RegularString);
}

// TODO: Refactor this things
std::u16string TextBlockText::ToU16String(const std::string& in) const
{
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utfConverter;
    return ToU16String(utfConverter.from_bytes(in));
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utfConverter;
    return utfConverter.from_bytes(in);
#endif // _WIN32
}

std::u16string TextBlockText::ToU16String(const std::wstring& in) const
{
    return {in.begin(), in.end()};
}

std::wstring TextBlockText::StringToWstring(const std::string& in) const
{
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utfConverter;
    return utfConverter.from_bytes(in);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utfConverter;
    return utfConverter.from_bytes(in);
#endif
}

std::string TextBlockText::WstringToString(const std::wstring& input) const
{
#ifdef _WIN32
    if (sizeof(wchar_t) == 2)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utfConverter;
        return utfConverter.to_bytes(input);
    }
    else
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utfConverter;
        return utfConverter.to_bytes(input);
    }
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utfConverter;
    return utfConverter.to_bytes(ToU16String(input));
#endif
}