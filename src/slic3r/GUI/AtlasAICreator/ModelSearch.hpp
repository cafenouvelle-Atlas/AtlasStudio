// Atlas AI Creator local search-link builder. SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>
#include <stdexcept>
namespace Slic3r::AtlasAICreator {
inline std::string encode_search_query(const std::string& query)
{
    if (query.empty() || query.size() > 1024)
        throw std::invalid_argument("Enter a search of 1 to 1024 UTF-8 bytes.");
    const char* hex = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : query) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            encoded += static_cast<char>(c);
        else {
            encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15];
        }
    }
    return encoded;
}
inline std::string makerworld_search_url(const std::string& query)
{
    return "https://makerworld.com/en/search/models?keyword=" + encode_search_query(query);
}
inline std::string stlbuddy_search_url(const std::string& query)
{
    return "https://stlbuddy.com/models/browse?q=" + encode_search_query(query) + "&sort=relevance";
}
}
