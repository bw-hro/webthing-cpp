// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <bw/webthing/mdns.hpp>
#include <bw/webthing/thing.hpp>
#include <uwebsockets/App.h>

namespace bw::webthing {

typedef uWS::SocketContextOptions SSLOptions;

#ifdef WT_WITH_SSL
    typedef uWS::SSLApp uWebsocketsApp;
#else
    typedef uWS::App uWebsocketsApp;
#endif

enum class ThingType { SingleThing, MultipleThings };

struct ThingContainer
{
    ThingContainer(std::vector<Thing*> things, std::string name, ThingType type)
        : things(things)
        , name(name)
        , type(type)
    {}

    std::string get_name() const
    {
        return name;
    }

    ThingType get_type () const
    {
        return type;
    } 

    std::optional<Thing*> get_thing(int index)
    {
        if(index >= things.size())
            return std::nullopt;

        if(type == ThingType::SingleThing)
            return things[0];

        return things[index];
    }

    std::vector<Thing*> get_things()
    {
        return things;
    }

protected:
    std::vector<Thing*> things;
    std::string name;
    ThingType type;
};

struct SingleThing : public ThingContainer
{
    SingleThing(Thing* thing)
        : ThingContainer({thing}, thing->get_title(), ThingType::SingleThing)
    {}
};

struct MultipleThings : public ThingContainer{
    MultipleThings(std::vector<Thing*> things, std::string name)
        : ThingContainer(things, name, ThingType::MultipleThings)
    {}
};

class WebThingServer
{
    struct Builder
    {
        Builder(ThingContainer things)
            : things_(things)
        {}

        Builder& port(int port)
        {
            port_ = port;
            return *this;
        }

        Builder& hostname(std::string hostname)
        {
            hostname_ = hostname;
            return *this;
        }
        
        Builder& base_path(std::string base_path)
        {
            base_path_ = base_path;
            return *this;
        }

        Builder& disable_host_validation(bool disable_host_validation)
        {
            disable_host_validation_ = disable_host_validation;
            return *this;
        }

        Builder& ssl_options(SSLOptions options)
        {
            ssl_options_ = options;
            return *this;
        }

        Builder& disable_mdns()
        {
            mdns_enabled_ = false;
            return *this;
        }

        WebThingServer build()
        {
            return WebThingServer(things_, port_, hostname_, base_path_, 
                disable_host_validation_, ssl_options_, mdns_enabled_);
        }

        void start()
        {
            build().start();
        }
    private:
        ThingContainer things_;
        int port_ = 80;
        std::optional<std::string> hostname_;
        /*std::vector<route> additional_routes;*/
        SSLOptions ssl_options_;
        std::string base_path_ = "/";
        bool disable_host_validation_ = false;
        bool mdns_enabled_ = true;

    };

public:
    static Builder host(ThingContainer things)
    {
        return Builder(things);
    }

    // disable copy, only allow move
    WebThingServer(const WebThingServer& other) = delete;

    WebThingServer(ThingContainer things, int port, std::optional<std::string> hostname, 
        std::string base_path, bool disable_host_validation, SSLOptions ssl_options = {}, bool enable_mdns = true)
        : things(things)
        , name(things.get_name())
        , port(port)
        , hostname(hostname)
        , base_path(base_path)
        , disable_host_validation(disable_host_validation)
        , ssl_options(ssl_options)
        , enable_mdns(enable_mdns)
    {
        if(this->base_path.back() == '/')
            this->base_path.pop_back();

        hosts.push_back("localhost");
        hosts.push_back("localhost:" + std::to_string(port));

        auto if_ips = get_addresses();
        for(auto& ip : if_ips)
        {
            hosts.push_back(ip);
            hosts.push_back(ip + ":" + std::to_string(port));
        }

        if(this->hostname)
        {
            auto hn = this->hostname.value();
            std::transform(hn.begin(), hn.end(), hn.begin(), ::tolower);
            this->hostname = hn;
            hosts.push_back(*this->hostname);
            hosts.push_back(*this->hostname + ":" + std::to_string(port));
        }

        initialize_webthing_routes();
    }

    void initialize_webthing_routes()
    {        
        web_server = std::make_unique<uWebsocketsApp>(ssl_options);
        auto& server = *web_server.get();

        bool is_single = things.get_type() == ThingType::SingleThing;
        
        int thing_index = -1;
        for(auto& thing : things.get_things())
        {
            thing_index++;
            thing->set_href_prefix(base_path + (is_single ? "" : "/" + std::to_string(thing_index)));
            thing->add_message_observer([this](auto topic, auto msg)
            {
                handle_thing_message(topic, msg);
            });
        }
        
        if(is_single)
        {
            server.get(base_path + "/", [&](auto* res, auto* req){handle_thing(res, req);});
            server.get(base_path + "/properties", [&](auto* res, auto* req){handle_properties(res, req);});
            server.get(base_path + "/properties/:property_name", [&](auto* res, auto* req){handle_property_get(res, req);});
            server.put(base_path + "/properties/:property_name", [&](auto* res, auto* req){handle_property_put(res, req);});
            server.get(base_path + "/actions", [&](auto* res, auto* req){handle_actions_get(res, req);});
            server.post(base_path + "/actions", [&](auto* res, auto* req){handle_actions_post(res, req);});
            server.get(base_path + "/actions/:action_name", [&](auto* res, auto* req){handle_actions_get(res, req);});
            server.post(base_path + "/actions/:action_name", [&](auto* res, auto* req){handle_actions_post(res, req);});
            server.get(base_path + "/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_get(res, req);});
            server.put(base_path + "/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_put(res, req);});
            server.del(base_path + "/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_delete(res, req);});
            server.get(base_path + "/events", [&](auto* res, auto* req){handle_events(res, req);});
            server.get(base_path + "/events/:event_name", [&](auto* res, auto* req){handle_events(res, req);});
        }
        else
        {
            server.get(base_path + "/", [&](auto* res, auto* req){handle_things(res, req);});
            server.get(base_path + "/:thing_id", [&](auto* res, auto* req){handle_thing(res, req);});
            server.get(base_path + "/:thing_id/properties", [&](auto* res, auto* req){handle_properties(res, req);});
            server.get(base_path + "/:thing_id/properties/:property_name", [&](auto* res, auto* req){handle_property_get(res, req);});
            server.put(base_path + "/:thing_id/properties/:property_name", [&](auto* res, auto* req){handle_property_put(res, req);});
            server.get(base_path + "/:thing_id/actions", [&](auto* res, auto* req){handle_actions_get(res, req);});
            server.post(base_path + "/:thing_id/actions", [&](auto* res, auto* req){handle_actions_post(res, req);});
            server.get(base_path + "/:thing_id/actions/:action_name", [&](auto* res, auto* req){handle_actions_get(res, req);});
            server.post(base_path + "/:thing_id/actions/:action_name", [&](auto* res, auto* req){handle_actions_post(res, req);});
            server.get(base_path + "/:thing_id/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_get(res, req);});
            server.put(base_path + "/:thing_id/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_put(res, req);});
            server.del(base_path + "/:thing_id/actions/:action_name/:action_id", [&](auto* res, auto* req){handle_action_id_delete(res, req);});
            server.get(base_path + "/:thing_id/events", [&](auto* res, auto* req){handle_events(res, req);});
            server.get(base_path + "/:thing_id/events/:event_name", [&](auto* res, auto* req){handle_events(res, req);});
        }
        server.any("/*", [&](auto* res, auto* req){handle_invalid_requests(res, req);});
        server.options("/*", [&](auto* res, auto* req){handle_options_requests(res, req);});

        for(auto& thing : things.get_things())
        {
            auto thing_id = thing->get_id();
            uWebsocketsApp::WebSocketBehavior<std::string> ws_behavior;
            ws_behavior.compression = uWS::SHARED_COMPRESSOR;
            ws_behavior.open = [thing_id](auto *ws)
            {
                std::string* ws_id = (std::string *) ws->getUserData();
                ws_id->append(generate_uuid());

                logger::debug("websocket open " + *ws_id);
                ws->subscribe(thing_id + "/properties");
                ws->subscribe(thing_id + "/actions");
            };
            ws_behavior.message = [thing_id, thing](auto *ws, std::string_view message, uWS::OpCode op_code)
            {
                logger::debug("websocket msg " + *((std::string*)ws->getUserData()) + ": " + std::string(message));
                json j;
                try
                {
                    j = json::parse(message);
                }
                catch (json::parse_error&)
                {
                    json error_message = {{"messageType", "error"}, {"data", {
                        {"status", "400 Bad Request"},
                        {"message", "Parsing request failed"}
                    }}};
                    ws->send(error_message.dump(), op_code);
                    return;
                }

                if(!j.contains("messageType") || !j.contains("data"))
                {
                    json error_message = {{"messageType", "error"}, {"data", {
                        {"status", "400 Bad Request"},
                        {"message", "Invalid message"}
                    }}};
                    ws->send(error_message.dump(), op_code);
                    return;
                }

                // e.g. {"messageType":"addEventSubscription", "data":{"eventName":{}}}
                std::string message_type = j["messageType"];
                if(message_type == "addEventSubscription")
                {
                    for(auto& evt : j["data"].items())
                        ws->subscribe(thing_id + "/events/" + evt.key());
                }
                else if(message_type == "setProperty")
                {
                    for(auto& property_entry : j["data"].items())
                    {
                        try
                        {
                            auto prop_setter = [&](auto val){
                                thing->set_property(property_entry.key(), val);
                            };

                            json v = property_entry.value();
                            if(v.is_boolean())
                                prop_setter(v.get<bool>());
                            else if(v.is_string())
                                prop_setter(v.get<std::string>());
                            else if(v.is_number_integer())
                                prop_setter(v.get<int>());                            
                            else if(v.is_number_unsigned())
                                prop_setter(v.get<unsigned int>());                            
                            else if(v.is_number_float())
                                prop_setter(v.get<double>());
                            else
                                prop_setter(v);
                        }
                        catch(std::exception& ex)
                        {
                            json error_message = {{"messageType", "error"}, {"data", {
                                {"status", "400 Bad Request"},
                                {"message", ex.what()}
                            }}};
                            ws->send(error_message.dump(), op_code);
                        }
                    }
                }
                else if(message_type == "requestAction")
                {
                    for(auto& action_entry : j["data"].items())
                    {
                        std::optional<json> input;
                        if(j["data"][action_entry.key()].contains("input"))
                            input = j["data"][action_entry.key()]["input"];
                        
                        auto action = thing->perform_action(action_entry.key(), std::move(input));
                        if(action)
                        {
                            std::thread action_runner([action]{
                                action->start();
                            });
                            action_runner.detach(); 
                        }
                    }
                }
                else
                {
                    json error_message = {{"messageType", "error"}, {"data", {
                        {"status", "400 Bad Request"},
                        {"message", "Unknown messageType: " + message_type},
                        {"request", message}
                    }}};
                    ws->send(error_message.dump(), op_code);
                }
            };
            ws_behavior.close = [thing_id](auto *ws, int /*code*/, std::string_view /*message*/)
            {
                logger::debug("websocket close " + *((std::string*)ws->getUserData()));
                ws->unsubscribe(thing_id + "/properties");
                ws->unsubscribe(thing_id + "/actions");
                ws->unsubscribe(thing_id + "/events/#");
            };

            server.ws<std::string>(thing->get_href(), std::move(ws_behavior));
        }

        server.listen(port, [&](auto *listen_socket) {
            if (listen_socket) {
                logger::info("Listening on port " + std::to_string(port));
            }
        });
    }

    void start()
    {
        logger::info("Start WebThingServer hosting '" + things.get_name() + 
            "' containing " + std::to_string(things.get_things().size()) + " thing" +
            std::string(things.get_things().size() == 1 ? "" : "s"));

        if(enable_mdns)
            start_mdns_service();

        webserver_loop = uWS::Loop::get();
        web_server->run();
        
        logger::info("Stopped WebThingServer hosting '" + things.get_name() + "'");
    }

    void stop()
    {
        logger::info("Stop WebThingServer hosting '" + things.get_name() + "'");
        
        if(enable_mdns)
            stop_mdns_service();

        web_server->close();
    }

    std::string get_name() const
    {
        return name;
    }

    uWebsocketsApp* get_web_server() const
    {
        return web_server.get();
    }

private:

    void start_mdns_service()
    {
        std::thread([this]{

            logger::info("Start mDNS service for WebThingServer hosting '" + things.get_name() + "'");

            #ifdef WT_WITH_SSL
                bool is_tls = true;
            #else
                bool is_tls = false;
            #endif

            mdns_service = std::make_unique<MdnsService>();
            mdns_service->start_service(things.get_name(), "_webthing._tcp.local.", port, base_path + "/", is_tls);

            logger::info("Stopped mDNS service for WebThingServer hosting '" + things.get_name() + "'");
  
        }).detach();
    }

    void stop_mdns_service()
    {
        using namespace std::chrono;

        if(mdns_service)
        {
            logger::info("Stop mDNS service for WebThingServer hosting '" + things.get_name() + "'");
            mdns_service->stop_service();

            auto timeout = milliseconds(5000);
            auto start = steady_clock::now();
            bool timeout_reached = false;
            while(mdns_service->is_running() || timeout_reached)
            {
                std::this_thread::sleep_for(milliseconds(1));
                auto current = steady_clock::now();
                timeout_reached = duration_cast<milliseconds>(current - start) >= timeout;
            }

        }
    }

    std::optional<Thing*> find_thing_from_url(uWS::HttpRequest* req)
    {
        if(things.get_type() == ThingType::SingleThing)
            return things.get_thing(0);

        auto thing_id_str = req->getParameter(0);
        try{
            int thing_id = std::stoi(thing_id_str.data());
            return things.get_thing(thing_id);
        }
        catch(std::exception&)
        {
            return std::nullopt;
        }
    }

    std::optional<std::string> find_param_after_thing_id_from_url(uWS::HttpRequest* req, int index_after_thing_id = 0)
    {
        int parameter_index = index_after_thing_id;
        if(things.get_type() == ThingType::MultipleThings)
            parameter_index += 1;

        auto param = req->getParameter(parameter_index);
        if(param.empty())
            return std::nullopt;

        return std::string(param);
    }

    std::optional<std::string> find_property_name_from_url(uWS::HttpRequest* req)
    {
        return find_param_after_thing_id_from_url(req);
    }

    std::optional<std::string> find_event_name_from_url(uWS::HttpRequest* req)
    {
        return find_param_after_thing_id_from_url(req);
    }

    std::optional<std::string> find_action_name_from_url(uWS::HttpRequest* req)
    {
        return find_param_after_thing_id_from_url(req, 0);
    }

    std::optional<std::string> find_action_id_from_url(uWS::HttpRequest* req)
    {
        return find_param_after_thing_id_from_url(req, 1);
    }

    bool validate_host(uWS::HttpRequest* req)
    {
        if(disable_host_validation)
            return true;
        
        std::string host(req->getHeader("host"));
        return std::find(hosts.begin(), hosts.end(), host) != hosts.end();
    }

    template<bool SSL>
    uWS::HttpResponse<SSL>* write_cors_response(uWS::HttpResponse<SSL>* response)
    {
        response->writeHeader("Access-Control-Allow-Origin", "*");
        response->writeHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
        response->writeHeader("Access-Control-Allow-Methods", "GET, HEAD, PUT, POST, DELETE");
        return response;
    }
    
    template<bool SSL>
    uWS::HttpResponse<SSL>* write_json_response(uWS::HttpResponse<SSL>* response)
    {
        response->writeHeader("Content-Type", "application/json");
        return response;
    }

    template<bool SSL>
    void handle_invalid_requests(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto host = req->getHeader("host");
        auto path = req->getUrl();
        
        if(path.back() == '/' && path != (base_path + "/"))
        {
            // redirect to non-trailing slash url
            res->writeStatus("301 Moved Permanently");
            res->writeHeader("Location", (SSL ? "https://" : "http://") + std::string(host) + std::string(path.data(), path.size()-1));
            write_cors_response(res);
            res->end();
        }
        else
        {
            res->writeStatus("405 Method Not Allowed");
            write_cors_response(res);
            res->end();
        }
    }

    template<bool SSL>
    void handle_options_requests(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        res->writeStatus("204 No Content");
        write_cors_response(res);
        res->end();
    }

    template<bool SSL>
    void base_handle_requests(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        if(!validate_host(req))
        {
            res->writeStatus("405 Method Not Allowed");
        }

        write_cors_response(res);
        write_json_response(res);
    }

    template<bool SSL>
    json prepare_thing_description(Thing* thing, uWS::HttpRequest* req)
    {
        std::string http_protocol = SSL ? "https" : "http";
        std::string ws_protocol = http_protocol == "https" ? "wss" : "ws";
        std::string host = std::string(req->getHeader("host"));
        std::string ws_href = ws_protocol + "://" + host;

        json desc = thing->as_thing_description();
        desc["href"] = thing->get_href();
        desc["links"].push_back({{"rel", "alternate"}, {"href", ws_href + thing->get_href()}});
        desc["base"] = http_protocol + "://" + host + thing->get_href();
        desc["securityDefinitions"] = {{"nosec_sc", {{"scheme", "nosec"}}}};
        desc["security"] = "nosec_sc";

        return desc;
    }

    template<bool SSL>
    void handle_things(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        json descriptions = json::array();
        
        for(auto thing : things.get_things())
        {
            json desc = prepare_thing_description<SSL>(thing, req);
            descriptions.push_back(desc);
        }
        
        base_handle_requests(res, req);
        res->end(descriptions.dump());
    }

    template<bool SSL>
    void handle_thing(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        json description = prepare_thing_description<SSL>(*thing, req);

        base_handle_requests(res, req);
        res->end(description.dump());
    }


    template<bool SSL>
    void handle_properties(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        base_handle_requests(res, req);
        res->end((*thing)->get_properties().dump());
    }

    template<bool SSL>
    void handle_property_get(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        auto property_name = find_property_name_from_url(req);

        if(!thing || !property_name)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        auto property = (*thing)->find_property(*property_name);

        if(!property)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        base_handle_requests(res, req);
        res->end(property->get_property_value_object().dump());
    }

    template<bool SSL>
    void handle_property_put(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        auto property_name_in_url = find_property_name_from_url(req);

        if(!thing || !property_name_in_url)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        auto property = (*thing)->find_property(*property_name_in_url);

        if(!property)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        res->onData([res, thing, property_name_in_url, property](std::string_view body_chunk, bool is_last)
        {
            if(is_last)
            {
                try
                {
                    if(body_chunk.empty())
                        throw PropertyError("Empty property request body");

                    std::string prop_name = *property_name_in_url;
                    json body = json::parse(body_chunk);

                    if(!body.contains(prop_name))
                        throw PropertyError("Property request body does not contain " + prop_name);

                    auto v = body[prop_name];
                    auto prop_setter = [&](auto val){
                        (*thing)->set_property(prop_name, val);
                    };

                    if(v.is_boolean())
                        prop_setter(v.get<bool>());
                    else if(v.is_string())
                        prop_setter(v.get<std::string>());
                    else if(v.is_number_integer())
                        prop_setter(v.get<int>());                            
                    else if(v.is_number_unsigned())
                        prop_setter(v.get<unsigned int>());                            
                    else if(v.is_number_float())
                        prop_setter(v.get<double>());
                    else
                        prop_setter(v);

                    res->end(property->get_property_value_object().dump());
                }
                catch(std::exception& ex)
                {
                    json body = {{"message", ex.what()}};
                    res->writeStatus("400 Bad Request")->end(body.dump());
                }
            }
        });

        res->onAborted([]{
            logger::debug("transfer request body aborted");
        });
    }

    // Handles GET requests to:
    // * /actions
    // * /actions/<action_name>
    template<bool SSL>
    void handle_actions_get(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        // can be std::nullopt which results in a collection of all actions
        auto action_name = find_action_name_from_url(req);

        base_handle_requests(res, req);
        res->end((*thing)->get_action_descriptions(action_name).dump());
    }


    // Handles POST requests to:
    // * /actions
    // * /actions/<action_name>
    template<bool SSL>
    void handle_actions_post(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        auto action_name_in_url = find_action_name_from_url(req);

        res->onData([res, thing, action_name_in_url](std::string_view body_chunk, bool is_last)
        {
            if(is_last)
            {
                try
                {
                    if(body_chunk.empty())
                        throw ActionError("Empty action request body");

                    json body = json::parse(body_chunk);
                    if(!body.is_object() || body.size() != 1 ||
                        (action_name_in_url && !body.contains(action_name_in_url)))
                        throw ActionError("Invalid action request body");

                    std::string action_name = action_name_in_url.value_or(body.begin().key());
                    json action_params = body[action_name];

                    std::optional<json> input;
                    if(action_params.contains("input"))
                        input = action_params["input"];

                    auto action = (*thing)->perform_action(action_name, std::move(input));
                    if(!action)
                        throw ActionError("Could not perform action");

                    json response = action->as_action_description();
                    std::thread action_runner([action]{
                        action->start();
                    });
                    action_runner.detach();

                    res->writeStatus("201 Created")->end(response.dump());

                }
                catch(std::exception& ex)
                {
                    json body = {{"message", ex.what()}};
                    res->writeStatus("400 Bad Request")->end(body.dump());
                }
            }
        });

        res->onAborted([]{
            logger::debug("transfer request body aborted");
        });

    }

    template<bool SSL>
    void handle_action_id_get(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        auto action_name = find_action_name_from_url(req);
        auto action_id = find_action_id_from_url(req);

        if(!thing || !action_name || !action_id)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        auto action = (*thing)->get_action(*action_name, *action_id);
        if(!action)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        base_handle_requests(res, req);
        res->end(action->as_action_description().dump());
    }

    // TODO: this is not yet defined in the spec
    // also cf. https://webthings.io/api/#actionrequest-resource
    template<bool SSL>
    void handle_action_id_put(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        base_handle_requests(res, req);
        res->end();
    }

    template<bool SSL>
    void handle_action_id_delete(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        auto action_name = find_action_name_from_url(req);
        auto action_id = find_action_id_from_url(req);

        if(!thing || !action_name || !action_id)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        auto action = (*thing)->get_action(*action_name, *action_id);
        if(!action)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        if(!(*thing)->remove_action(*action_name, *action_id))
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        res->writeStatus("204 No Content");
        base_handle_requests(res, req);
        res->end();
    }

    // Handles requests to:
    // * /events
    // * /events/<event_name>
    template<bool SSL>
    void handle_events(uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req)
    {
        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            res->writeStatus("404 Not Found")->end();
            return;
        }

        // can be std::nullopt which results in a collection of all events
        auto event_name = find_event_name_from_url(req);

        base_handle_requests(res, req);
        res->end((*thing)->get_event_descriptions(event_name).dump());
    }

    // forward thing messages to servers websocket clients
    void handle_thing_message(const std::string& topic, const json& message)
    {
        if(!webserver_loop)
            return;

        std::string t = topic;
        std::string m = message.dump();

        webserver_loop->defer([this, t, m]{
            logger::debug("server broadcast : " + t + " : " + m);
            web_server->publish(t, m, uWS::OpCode::TEXT);
        });
    }

    ThingContainer things;
    int port;
    std::string name;
    std::optional<std::string> hostname;
    /*std::vector<route> additional_routes;*/
    SSLOptions ssl_options;
    std::string base_path = "/";
    bool disable_host_validation = false;
    bool enable_mdns = true;

    std::vector<std::string> hosts;

    uWS::Loop* webserver_loop; // Must be initialized from thread that calls start()
    std::unique_ptr<uWebsocketsApp> web_server;
    std::unique_ptr<MdnsService> mdns_service;
};

} // bw::webthing