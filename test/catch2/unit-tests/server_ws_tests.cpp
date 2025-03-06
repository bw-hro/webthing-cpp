// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <catch2/catch_all.hpp>
#include <cpr/cpr.h>
#include <ixwebsocket/IXWebSocket.h>
#include <bw/webthing/webthing.hpp>

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

void connect_via_ws(const std::string& url, std::function<void (ix::WebSocket*, std::vector<json>*)> client_callback)
{
    std::vector<json> messages_received;

    ix::WebSocket ws;
    ws.setUrl(url);
    ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg)
    {
        if (msg->type == ix::WebSocketMessageType::Message)
        {
            std::string message = msg->str;
            logger::info("WS_CLIENT RECEIVED: " + message);
            messages_received.push_back(json::parse(message));
        }
    });

    ws.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client_callback(&ws, &messages_received);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ws.stop();
}

TEST_CASE( "It can make a single thing via websocket available", "[server][ws]" )
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
    auto builder = WebThingServer::host(thing_container).port(57111);

    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "single-thing");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["title"] == "single-thing");
        
        auto links = td["links"];
        auto found_ws_link_obj = std::find_if(links.begin(), links.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" && !link.contains("mediaType");
        });
        REQUIRE(found_ws_link_obj != links.end());
        std::string ws_url = (*found_ws_link_obj)["href"];

        connect_via_ws(ws_url, [&](auto con, std::vector<json>* received_messages_ptr)
        {
            std::vector<json>& received_messages = *received_messages_ptr;

            res = cpr::Put(
                cpr::Url{base_url + "/properties/brightness"}, 
                cpr::Body{json{{"brightness", 42}}.dump()}
            );
            REQUIRE(res.status_code == 200);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["brightness"] == 42);

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"brightness", 24}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["brightness"] == 24);

            // send no json message
            con->sendText("Some string not beeing json...");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "error");
            REQUIRE(received_messages.back()["data"]["status"] == "400 Bad Request");
            REQUIRE(received_messages.back()["data"]["message"] == "Parsing request failed");

            // send json message missing messageType
            con->sendText(json{{"data", {{"brightness", 666}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "error");
            REQUIRE(received_messages.back()["data"]["status"] == "400 Bad Request");
            REQUIRE(received_messages.back()["data"]["message"] == "Invalid message");

            // send json message missing data
            con->sendText(json{{"messageType", "setProperty"}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "error");
            REQUIRE(received_messages.back()["data"]["status"] == "400 Bad Request");
            REQUIRE(received_messages.back()["data"]["message"] == "Invalid message");

            // send json message with wrong data type
            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"brightness", "some-unexpected-string"}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "error");
            REQUIRE(received_messages.back()["data"]["status"] == "400 Bad Request");
            REQUIRE(received_messages.back()["data"]["message"] == "Property value type not matching");

            // send json message with invalid messageType
            con->sendText(json{{"messageType", "invalidCommand"}, {"data", {{"perform", "invalid-task"}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "error");
            REQUIRE(received_messages.back()["data"]["status"] == "400 Bad Request");
            REQUIRE(received_messages.back()["data"]["message"] == "Unknown messageType: invalidCommand");
        });

        res = cpr::Get(cpr::Url{base_url + "/properties"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["brightness"] == 24);
    });
}

TEST_CASE( "It can make multiple things via websocket available", "[server][ws]" )
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

    auto builder = WebThingServer::host(thing_container).port(57112);
    test_running_server(builder, [](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "things-a-and-b");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);

        auto links_a = td[0]["links"];
        auto found_ws_link_a_obj = std::find_if(links_a.begin(), links_a.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" && !link.contains("mediaType");
        });
        REQUIRE(found_ws_link_a_obj != links_a.end());
        std::string ws_url_a = (*found_ws_link_a_obj)["href"];


        auto links_b = td[1]["links"];
        auto found_ws_link_b_obj = std::find_if(links_b.begin(), links_b.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" && !link.contains("mediaType");
        });
        REQUIRE(found_ws_link_b_obj != links_b.end());
        std::string ws_url_b = (*found_ws_link_b_obj)["href"];


        connect_via_ws(ws_url_a, [&](auto con, std::vector<json>* received_messages_ptr)
        {
            std::vector<json>& received_messages = *received_messages_ptr;

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"boolean-prop", false}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["boolean-prop"] == false);

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"double-prop", 24.0}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["double-prop"] == 24.0);

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"string-prop", "the-updated-value"}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["string-prop"] == "the-updated-value");
        });

        res = cpr::Get(cpr::Url{base_url + "/0/properties"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["boolean-prop"] == false);
        REQUIRE(json::parse(res.text)["double-prop"] == 24.0);
        REQUIRE(json::parse(res.text)["string-prop"] == "the-updated-value");

        connect_via_ws(ws_url_b, [&](auto con, std::vector<json>* received_messages_ptr)
        {
            std::vector<json>& received_messages = *received_messages_ptr;

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"object-prop", {{"key", "updated-value"}}}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["object-prop"] == json{{"key", "updated-value"}});

            con->sendText(json{{"messageType", "setProperty"}, {"data", {{"array-prop", {"a", "b", "c", 42}}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "propertyStatus");
            REQUIRE(received_messages.back()["data"]["array-prop"] == json{"a", "b", "c", 42});
        });

        res = cpr::Get(cpr::Url{base_url + "/1/properties"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text)["object-prop"] == json{{"key", "updated-value"}});
        REQUIRE(json::parse(res.text)["array-prop"] == json{"a", "b", "c", 42});
    });
}


TEST_CASE( "It offers websocket api for events", "[server][ws]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");
    link_event(thing, "count-event", {{"title", "Count Event"}, {"type", "number"}});
    link_event(thing, "message-event", {{"title", "Message Event"}, {"type", "string"}});

    auto thing_container = SingleThing(thing.get());
    auto builder = WebThingServer::host(thing_container).port(57113);

    test_running_server(builder, [&](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "single-thing");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["title"] == "single-thing");

        res = cpr::Get(cpr::Url{base_url + "/events"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        auto links = td["links"];
        auto found_ws_link_obj = std::find_if(links.begin(), links.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" && !link.contains("mediaType");
        });
        REQUIRE(found_ws_link_obj != links.end());
        std::string ws_url = (*found_ws_link_obj)["href"];

        connect_via_ws(ws_url, [&](auto con, std::vector<json>* received_messages_ptr)
        {
            std::vector<json>& received_messages = *received_messages_ptr;

            // event emitted without websocket subscribers 
            emit_event(thing, "count-event", 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.empty());

            // subscribe to events
            con->sendText(json{{"messageType", "addEventSubscription"}, {"data", {{"count-event", json::object()}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            con->sendText(json{{"messageType", "addEventSubscription"}, {"data", {{"message-event", json::object()}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // events emitted with websocket subscribers
            emit_event(thing, "count-event", 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "event");
            REQUIRE(received_messages.back()["data"]["count-event"]["data"] == 1);

            emit_event(thing, "message-event", "msg-a");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "event");
            REQUIRE(received_messages.back()["data"]["message-event"]["data"] == "msg-a");

            emit_event(thing, "count-event", 2);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            REQUIRE(received_messages.back()["messageType"] == "event");
            REQUIRE(received_messages.back()["data"]["count-event"]["data"] == 2);
        });

        res = cpr::Get(cpr::Url{base_url + "/events"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 4);
    });
}


TEST_CASE( "It offers websocket api for actions", "[server][ws]" )
{
    auto thing = make_thing("uri:test:1", "single-thing");

    link_action(thing, "test-action", json{{"title", "Test Action"}}, []{
        logger::info("PERFORM TEST ACTION");
    });

    auto thing_container = SingleThing(thing.get());
    auto builder = WebThingServer::host(thing_container).port(57114);

    test_running_server(builder, [&](WebThingServer* server, const std::string& base_url)
    {
        REQUIRE(server->get_name() == "single-thing");
        auto res = cpr::Get(cpr::Url{base_url});
        REQUIRE(res.status_code == 200);
        auto td = json::parse(res.text);
        REQUIRE(td["title"] == "single-thing");

        res = cpr::Get(cpr::Url{base_url + "/actions"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).empty());

        auto links = td["links"];
        auto found_ws_link_obj = std::find_if(links.begin(), links.end(), [](const json& link) {
            return link.contains("rel") && link["rel"] == "alternate" && !link.contains("mediaType");
        });
        REQUIRE(found_ws_link_obj != links.end());
        std::string ws_url = (*found_ws_link_obj)["href"];

        connect_via_ws(ws_url, [&](auto con, std::vector<json>* received_messages_ptr)
        {
            std::vector<json>& received_messages = *received_messages_ptr;

            con->sendText(json{{"messageType", "requestAction"}, {"data", {{"test-action", {{"input", 42}}}}}}.dump());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            REQUIRE(received_messages.size() == 3);
            
            REQUIRE(received_messages[0]["messageType"] == "actionStatus");
            REQUIRE(received_messages[0]["data"]["test-action"]["status"] == "created");

            REQUIRE(received_messages[1]["messageType"] == "actionStatus");
            REQUIRE(received_messages[1]["data"]["test-action"]["status"] == "pending");

            REQUIRE(received_messages[2]["messageType"] == "actionStatus");
            REQUIRE(received_messages[2]["data"]["test-action"]["status"] == "completed");
        });

        res = cpr::Get(cpr::Url{base_url + "/actions"});
        REQUIRE(res.status_code == 200);
        REQUIRE(json::parse(res.text).size() == 1);
    });
}
