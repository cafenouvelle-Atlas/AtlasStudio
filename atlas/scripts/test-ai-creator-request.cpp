// SPDX-License-Identifier: AGPL-3.0-only
#include "slic3r/GUI/AtlasAICreator/PrimitiveRequest.hpp"
#include "slic3r/GUI/AtlasAICreator/ModelSearch.hpp"
#include <iostream>
#include <limits>

using namespace Slic3r::AtlasAICreator;
int main()
{
    int checks = 0;
    auto expect = [&](PrimitiveRequest request, bool accepted) {
        bool valid = true;
        try { validate(request); } catch (const std::invalid_argument&) { valid = false; }
        ++checks;
        if (valid != accepted) throw std::runtime_error("Unexpected dimension validation result");
    };
    for (auto shape : {Primitive::Box, Primitive::Cylinder, Primitive::Sphere}) {
        expect({shape, 20, 30, 40}, true);
        expect({shape, 0.1, 0.1, 0.1}, true);
        expect({shape, 1000, 1000, 1000}, true);
        for (double invalid : {0.0, -1.0, 0.09, 1000.01,
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
            expect({shape, invalid, 20, 20}, false);
            if (shape == Primitive::Box) expect({shape, 20, invalid, 20}, false);
            if (shape != Primitive::Sphere) expect({shape, 20, 20, invalid}, false);
        }
    }
    expect({Primitive::Sphere, 20, 0, 0}, true);
    expect({Primitive::Cylinder, 20, 0, 20}, true);
    expect({static_cast<Primitive>(99), 20, 20, 20}, false);
    if (makerworld_search_url("phone stand & x#?") !=
        "https://makerworld.com/en/search/models?keyword=phone%20stand%20%26%20x%23%3F")
        throw std::runtime_error("Search query was not encoded safely");
    if (makerworld_search_url("https://evil.example") !=
        "https://makerworld.com/en/search/models?keyword=https%3A%2F%2Fevil.example")
        throw std::runtime_error("Search destination changed");
    if (stlbuddy_search_url("phone stand & x#?") !=
        "https://stlbuddy.com/models/browse?q=phone%20stand%20%26%20x%23%3F&sort=relevance")
        throw std::runtime_error("STL Buddy query was not encoded safely");
    for (const auto& query : {std::string(), std::string(1025, 'x')}) {
        bool rejected = false;
        try { makerworld_search_url(query); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Invalid search length accepted");
    }
    std::cout << "PASS: search URL encoding, fixed destinations and length limits\n";
    std::cout << "PASS: " << checks << " primitive request validation checks\n";
}
