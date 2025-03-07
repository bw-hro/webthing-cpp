// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <cpr/cpr.h>
#include <bw/webthing/webthing.hpp>
#include <bw/webthing/server.hpp>

using namespace bw::webthing;

inline void test_running_server(WebThingServer::Builder& builder, std::function<void (WebThingServer*, const std::string&)> test_callback)
{
    logger::set_level(log_level::trace);
    auto server = builder.build();
    REQUIRE(server.get_web_server() != nullptr);

    int port = server.get_port();
    std::string base_path = server.get_base_path();
    std::exception_ptr thread_exception = nullptr;

    auto t = std::thread([&](){

        try
        {
            std::string base_url = "http://localhost:" + std::to_string(port) + base_path;
            test_callback(&server, base_url);
        } catch (...) {
            thread_exception = std::current_exception();
        }

        server.stop();
    });

    server.start();
    t.join();

    if (thread_exception) {
        std::rethrow_exception(thread_exception);
    }
};

TEST_CASE( "It can host a single thing", "[server][http]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");
    link_property(thing, "brightness", 50, {
        {"@type", "BrightnessProperty"},
        {"title", "Brightness"},
        {"type", "integer"},
        {"description", "The level of light from 0-100"},
        {"minimum", 0},
        {"maximum", 100},
        {"unit", "percent"}});
        
    auto thing_container = SingleThing(thing.get());
    auto builder = WebThingServer::host(thing_container).port(57456);

    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "single-thing");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["title"] == "single-thing");

        res = cpr::Get(cpr::Url{base_url + "/properties"});
        REQUIRE(res.status_code == 200);
        auto props = json::parse(res.text);
        REQUIRE(props["brightness"] == 50);

        res = cpr::Put(
            cpr::Url{base_url + "/properties/brightness"}, 
            cpr::Body{json{{"brightness", 42}}.dump()}
        );
        REQUIRE(res.status_code == 200);

        res = cpr::Get(cpr::Url{base_url + "/properties/brightness"});
        REQUIRE(res.status_code == 200);
        props = json::parse(res.text);
        REQUIRE(props["brightness"] == 42);

        res = cpr::Put(
            cpr::Url{base_url + "/properties/brightness"} /*no body*/
        );
        REQUIRE(res.status_code == 400); // bad request

        // property name missing in payload
        res = cpr::Put(
            cpr::Url{base_url + "/properties/brightness"},
            cpr::Body{json{123}.dump()}
        );
        REQUIRE(res.status_code == 400); // bad request

        res = cpr::Get(cpr::Url{base_url + "/properties/not-existing-property"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Put(
            cpr::Url{base_url + "/properties/not-existing-property"}, 
            cpr::Body{json{{"not-existing-property", 123}}.dump()}
        );
        REQUIRE(res.status_code == 404); // not found
    });
}

TEST_CASE( "It can host multiple things", "[server][http]" )
{
    auto thing_a = make_thing("uri:test:a", "thing-a");
    
    link_property(thing_a, "boolean-prop", true, {
        {"title", "Bool Property"},
        {"type", "boolean"}
    });
    
    link_property(thing_a, "double-prop", 42.13, {
        {"title", "Double Property"},
        {"type", "number"}
    });

    link_property(thing_a, "string-prop", std::string("the-value"), {
        {"title", "String Property"},
        {"type", "string"}
    });

    auto thing_b = make_thing("uri:test:b", "thing-b");

    link_property(thing_b, "object-prop", json{{"key", "value"}}, {
        {"title", "Object Property"},
        {"type", "object"}
    });

    link_property(thing_b, "array-prop", json{"some", "values", 42}, {
        {"title", "Array Property"},
        {"type", "array"}
    });

    auto thing_container = MultipleThings({thing_a.get(), thing_b.get()}, "things-a-and-b");

    auto builder = WebThingServer::host(thing_container).port(57123);
    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "things-a-and-b");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);

        res = cpr::Get(cpr::Url{base_url + "/0"});
        REQUIRE(res.status_code == 200);
        auto ta = json::parse(res.text);
        REQUIRE(ta["title"] == "thing-a");

        res = cpr::Get(cpr::Url{base_url + "/1"});
        REQUIRE(res.status_code == 200);
        auto tb = json::parse(res.text);
        REQUIRE(tb["title"] == "thing-b");

        res = cpr::Put(
            cpr::Url{base_url + "/0/properties/boolean-prop"}, 
            cpr::Body{json{{"boolean-prop", false}}.dump()}
        );
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["boolean-prop"] == false);

        res = cpr::Put(
            cpr::Url{base_url + "/0/properties/double-prop"}, 
            cpr::Body{json{{"double-prop", 24.0}}.dump()}
        );
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["double-prop"] == 24.0);

        res = cpr::Put(
            cpr::Url{base_url + "/0/properties/string-prop"}, 
            cpr::Body{json{{"string-prop", "the-updated-value"}}.dump()}
        );
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["string-prop"] == "the-updated-value");

        res = cpr::Put(
            cpr::Url{base_url + "/1/properties/object-prop"}, 
            cpr::Body{json{{"object-prop", {{"key", "updated-value"}}}}.dump()}
        );
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["object-prop"] == json{{"key", "updated-value"}});

        res = cpr::Put(
            cpr::Url{base_url + "/1/properties/array-prop"}, 
            cpr::Body{json{{"array-prop", {"a", "b", "c", 42}}}.dump()}
        );
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["array-prop"] == json{"a", "b", "c", 42});

        // not exsiting thing
        res = cpr::Get(cpr::Url{base_url + "/42"});
        REQUIRE(res.status_code == 404);

        res = cpr::Get(cpr::Url{base_url + "/42/properties"});
        REQUIRE(res.status_code == 404);

        res = cpr::Get(cpr::Url{base_url + "/42/properties/test-property"});
        REQUIRE(res.status_code == 404);

        res = cpr::Put(
            cpr::Url{base_url + "/42/properties/test-property"}, 
            cpr::Body{json{{"key", "value"}}.dump()}
        );
        REQUIRE(res.status_code == 404);
    });
}

TEST_CASE( "It handles invalid requests", "[server][http]" )
{
    Thing thing("uri:test", "single-thing");
    auto thing_container = SingleThing(&thing);

    auto builder = WebThingServer::host(thing_container).port(57111);
    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);

        res = cpr::Put(cpr::Url{base_url});
        REQUIRE(res.status_code == 405); // method not allowed

        res = cpr::Get(cpr::Url{base_url + "/some-not-existing-resource"});
        REQUIRE(res.status_code == 405); // method not allowed

        res = cpr::Get(cpr::Url{base_url + "/properties/not-existing-property"});
        REQUIRE(res.status_code == 404); // not
    });
}

TEST_CASE( "It supports preflight requests", "[server][http]" )
{
    Thing thing("uri:test", "single-thing");
    auto thing_container = SingleThing(&thing);

    auto builder = WebThingServer::host(thing_container).port(57222);
    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Options(cpr::Url{base_url});
        REQUIRE(res.status_code == 204); // no content
    });
}

TEST_CASE( "It redirects urls with trailing slash to corresponding url without trailing slash", "[server][http]" )
{
    Thing thing("uri:test", "single-thing");
    auto thing_container = SingleThing(&thing);

    auto builder = WebThingServer::host(thing_container).port(57222);
    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Get(cpr::Url{base_url + "/properties/"}, cpr::Redirect{false});
        REQUIRE(res.status_code == 301); // moved permanently
        REQUIRE(res.header["Location"] == base_url + "/properties");
    });
}

TEST_CASE( "It supports custom host name", "[server][http]" )
{
    Thing thing("uri:test", "single-thing");
    auto thing_container = SingleThing(&thing);

    // test with enabled host validation
    auto builder_host_validation = WebThingServer::host(thing_container)
        .hostname("custom-host")
        .disable_host_validation(false)
        .port(57333);

    test_running_server(builder_host_validation, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Get(cpr::Url{base_url}, cpr::Header{{"Host","custom-host"}});
        REQUIRE(res.status_code == 200);

        res = cpr::Get(cpr::Url{base_url}, cpr::Header{{"Host","unknown-host"}});
        REQUIRE(res.status_code == 403); // forbidden
    });

    // test with disabled host validation
    auto builder_no_host_validation = WebThingServer::host(thing_container)
        .hostname("custom-host")
        .disable_host_validation(true)
        .port(57334);

    test_running_server(builder_no_host_validation, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Get(cpr::Url{base_url}, cpr::Header{{"Host","custom-host"}});
        REQUIRE(res.status_code == 200);

        res = cpr::Get(cpr::Url{base_url}, cpr::Header{{"Host","unknown-host"}});
        REQUIRE(res.status_code == 200);
    });
}

TEST_CASE( "It supports custom base path", "[server][http]" )
{
    Thing thing("uri:test", "single-thing");
    auto thing_container = SingleThing(&thing);

    auto builder = WebThingServer::host(thing_container)
        .base_path("/custom-base")
        .port(57444);

    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        REQUIRE_THAT(base_url, Catch::Matchers::EndsWith("/custom-base"));
        REQUIRE_THAT(json::parse(res.text)["base"], Catch::Matchers::EndsWith("/custom-base"));
    });
}

TEST_CASE( "It offers REST api for actions", "[server][http]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");

    link_action(thing, "test-action", json{{"title", "Test Action"}}, []{
        logger::info("PERFORM TEST ACTION");
    });

    link_action(thing, "throwing-test-action", json{
        {"title", "Throwing Test Action"},
        {"input", {{"type","number"}}}
    }, []{
        logger::info("PERFORM THROWING TEST ACTION");
        throw std::runtime_error("ACTION FAILED BY INTEND");
    }); 

    auto thing_container = MultipleThings({thing.get()}, "single-thing-in-multi-container");
    auto builder = WebThingServer::host(thing_container).port(57777);

    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        std::string thing_base_url = base_url + "/0";

        REQUIRE(server->get_name() == "single-thing-in-multi-container");
        auto res = cpr::Get(cpr::Url{thing_base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["actions"]["test-action"]["title"] == "Test Action");

        res = cpr::Get(cpr::Url{thing_base_url + "/actions"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        res = cpr::Get(cpr::Url{thing_base_url + "/actions/test-action"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        res = cpr::Post(
            cpr::Url{thing_base_url + "/actions"}, 
            cpr::Body{json{{"test-action", {{"input", 42}}}}.dump()}
        );
        REQUIRE(res.status_code == 201);
        auto a1 = json::parse(res.text);
        REQUIRE(a1["test-action"].contains("href"));

        std::string a1_href = a1["test-action"]["href"];
        res = cpr::Get(cpr::Url{base_url + a1_href});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["test-action"]["input"] == 42);

        res = cpr::Get(cpr::Url{thing_base_url + "/actions"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 1);

        res = cpr::Post(
            cpr::Url{thing_base_url + "/actions"}, 
            cpr::Body{json{{"test-action", {{"input", 123}}}}.dump()}
        );
        REQUIRE(res.status_code == 201);
        auto a2 = json::parse(res.text);
        REQUIRE(a2["test-action"].contains("href"));

        res = cpr::Get(cpr::Url{thing_base_url + "/actions/test-action"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 2);

        res = cpr::Put(cpr::Url{base_url + a1_href});
        REQUIRE(res.status_code == 200);

        res = cpr::Delete(cpr::Url{base_url + a1_href});
        REQUIRE(res.status_code == 204); // no content

        // action already deleted
        res = cpr::Get(cpr::Url{base_url + a1_href});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Get(cpr::Url{thing_base_url + "/actions"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 1);
        REQUIRE(json::parse(res.text)[0]["test-action"]["input"] == 123);

        res = cpr::Delete(cpr::Url{base_url + a1_href});
        REQUIRE(res.status_code == 404); // not found
        
        res = cpr::Get(cpr::Url{thing_base_url + "/actions/not-existing-action/123-456"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Delete(cpr::Url{thing_base_url + "/actions/not-existing-action/123-456"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Post(
            cpr::Url{thing_base_url + "/actions"} /*no body*/
        );
        REQUIRE(res.status_code == 400); // bad request

        res = cpr::Post(
            cpr::Url{thing_base_url + "/actions/test-action"}, 
            cpr::Body{json{{"invalid-action-body", {{"foo", "bar"}}}}.dump()}
        );
        REQUIRE(res.status_code == 400); // bad request

        res = cpr::Post(
            cpr::Url{thing_base_url + "/actions/throwing-test-action"}, 
            cpr::Body{json{{"throwing-test-action", {{"input", "some-string-but-number-expected"}}}}.dump()}
        );
        REQUIRE(res.status_code == 400); // bad request

        // thing not existing
        res = cpr::Get(cpr::Url{base_url + "/42/actions/test-action"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Get(cpr::Url{base_url + "/42/actions/test-action/123-456"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Delete(cpr::Url{base_url + "/42/actions/test-action/123-456"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Put(cpr::Url{base_url + "/42/actions/test-action/123-456"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Post(
            cpr::Url{base_url + "/42/actions/test-action"} /*no body*/
        );
        REQUIRE(res.status_code == 404); // not found
    });
}

TEST_CASE( "It offers REST api for events", "[server][http]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");
    link_event(thing, "count-event", {{"title", "Count Event"}, {"type", "number"}});
    link_event(thing, "message-event", {{"title", "Message Event"}, {"type", "string"}});

    auto thing_container = MultipleThings({thing.get()}, "single-thing-in-multi-container");
    auto builder = WebThingServer::host(thing_container).port(57888);

    test_running_server(builder, [&thing](WebThingServer* server, const std::string& base_url)
    {
        std::string thing_base_url = base_url + "/0";

        REQUIRE(server->get_name() == "single-thing-in-multi-container");
        auto res = cpr::Get(cpr::Url{thing_base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["events"]["count-event"]["title"] == "Count Event");
        REQUIRE(td["events"]["message-event"]["title"] == "Message Event");

        res = cpr::Get(cpr::Url{thing_base_url + "/events"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        res = cpr::Get(cpr::Url{thing_base_url + "/events/count-event"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        emit_event(thing, "count-event", 1);
        res = cpr::Get(cpr::Url{thing_base_url + "/events/count-event"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 1);
        REQUIRE(json::parse(res.text)[0]["count-event"]["data"] == 1);

        emit_event(thing, "message-event", "msg-a");
        res = cpr::Get(cpr::Url{thing_base_url + "/events/message-event"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 1);
        REQUIRE(json::parse(res.text)[0]["message-event"]["data"] == "msg-a");

        emit_event(thing, "count-event", 2);
        res = cpr::Get(cpr::Url{thing_base_url + "/events/count-event"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 2);
        REQUIRE(json::parse(res.text)[1]["count-event"]["data"] == 2);

        res = cpr::Get(cpr::Url{thing_base_url + "/events"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 3);
        REQUIRE(json::parse(res.text)[0]["count-event"]["data"] == 1);
        REQUIRE(json::parse(res.text)[1]["message-event"]["data"] == "msg-a");
        REQUIRE(json::parse(res.text)[2]["count-event"]["data"] == 2);

        // thing not existing
        res = cpr::Get(cpr::Url{base_url + "/42/events"});
        REQUIRE(res.status_code == 404); // not found

        res = cpr::Get(cpr::Url{base_url + "/42/events/test-event"});
        REQUIRE(res.status_code == 404); // not found
    });
}

TEST_CASE( "It supports custom html ui page", "[server][http]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");
    REQUIRE_FALSE(thing->get_ui_href());

    thing->set_ui_href("/gui.html");
    REQUIRE(thing->get_ui_href() == "/gui.html");


    auto thing_container = MultipleThings({thing.get()}, "single-thing-in-multi-container");
    auto builder = WebThingServer::host(thing_container).port(57999);

    test_running_server(builder, [&thing](WebThingServer* server, const std::string& base_url)
    {
        auto web = server->get_web_server();

        // register additional html page
        web->get("/gui.html", [&](auto res, auto req){
            WebThingServer::Response(req, res).html("<h1>It works...</h1>").end();
        });

        std::string thing_base_url = base_url + "/0";

        REQUIRE(server->get_name() == "single-thing-in-multi-container");
        auto res = cpr::Get(cpr::Url{thing_base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        auto links = td["links"];

        auto found_gui_link_obj = std::find_if(links.begin(), links.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" &&
                   link.contains("mediaType") && link["mediaType"] == "text/html";
        });

        REQUIRE(found_gui_link_obj != links.end());

        std::string ui_href = (*found_gui_link_obj)["href"];
        REQUIRE(ui_href == "/gui.html");

        res = cpr::Get(cpr::Url{base_url + ui_href});
        REQUIRE(res.status_code == 200);
        REQUIRE_THAT(res.header["Content-Type"], Catch::Matchers::StartsWith("text/html"));
        REQUIRE(res.text == "<h1>It works...</h1>");
    });
}
