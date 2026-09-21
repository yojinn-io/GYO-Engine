#include "doctest/doctest.h"

#include "engine/asset/catalog/CatalogParser.hpp"

using Engine::Asset::Catalog::CatalogParser;

TEST_CASE("CatalogParser: valid json") {
    CatalogParser p;
    const char* json = R"({
      "assets":[
        {"id":"ui.title","type":"text","path":"ui/title.txt"},
        {"id":"sfx.hit","type":"sound","path":"audio/hit.wav"}
      ]
    })";

    auto r = p.Parse(json, "mem://catalog.json");
    CHECK(r);
    CHECK(r.value().size() == 2);
    CHECK(r.value()[0].id == "ui.title");
    CHECK(r.value()[0].type == "text");
    CHECK(r.value()[0].path == "ui/title.txt");
}

TEST_CASE("CatalogParser: invalid schema") {
    CatalogParser p;
    const char* json = R"({"foo":123})";
    auto r = p.Parse(json, "mem://catalog.json");
    CHECK(!r);
    CHECK(r.error().code == Engine::Asset::AssetErrorCode::ParseFailed);
}

TEST_CASE("CatalogParser: missing fields") {
    CatalogParser p;
    const char* json = R"({ "assets":[ {"id":"a","type":"text"} ] })";
    auto r = p.Parse(json, "mem://catalog.json");
    CHECK(!r);
    CHECK(r.error().code == Engine::Asset::AssetErrorCode::InvalidCatalogEntry);
}
