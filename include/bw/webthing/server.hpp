// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <bw/webthing/mdns.hpp>
#include <bw/webthing/thing.hpp>
#include <bw/webthing/version.hpp>
#include <uwebsockets/App.h>

namespace bw::webthing {

typedef uWS::SocketContextOptions SSLOptions;

constexpr bool is_ssl_enabled()
{
#ifdef WT_WITH_SSL
    return true;
#else
    return false;
#endif
}

#ifdef WT_WITH_SSL
    typedef uWS::SSLApp uWebsocketsApp;
#else
    typedef uWS::App uWebsocketsApp;
#endif

typedef uWS::HttpResponse<is_ssl_enabled()> uwsHttpResponse;


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

        Builder& limit_memory()
        {
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
    struct Response
    {
        Response(uWS::HttpRequest* req, uwsHttpResponse* res)
            : req_(req)
            , res_(res)
        {}

        Response& status(std::string_view status)
        {
            status_ = status;
            return *this;
        }

        Response& body(std::string_view body)
        {
            body_ = body;
            return *this;
        }

        Response& bad_request()
        {
            return status("400 Bad Request");
        }

        Response& forbidden()
        {
            return status("403 Forbidden");
        }

        Response& not_found()
        {
            return status("404 Not Found");
        }

        Response& method_not_allowed()
        {
            return status("405 Method Not Allowed");
        }

        Response& moved_permanently()
        {
            return status("301 Moved Permanently");
        }

        Response& no_content()
        {
            return status("204 No Content");
        }

        Response& created()
        {
            return status("201 Created");
        }

        Response& header(std::string_view key, std::string_view value)
        {
            headers_[key] = value;
            return *this;
        }

        Response& cors()
        {
            header("Access-Control-Allow-Origin", "*");
            header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
            header("Access-Control-Allow-Methods", "GET, HEAD, PUT, POST, DELETE");
            return *this;
        }

        Response& json(std::string_view body)
        {
            this->header("Content-Type", "application/json");
            this->body(body);
            return *this;
        }

        Response& html(std::string_view body)
        {
            this->header("Content-Type", "text/html; charset=utf-8");
            this->body(body);
            return *this;
        }


        void end()
        {
            cors();

            res_->writeStatus(status_);
            for(const auto& kv : headers_)
            {
                res_->writeHeader(kv.first, kv.second);
            }
            res_->end(body_);

            if(logger::get_level() == log_level::trace)
            {
                std::stringstream ss;
                ss << "http - '" << res_->getRemoteAddressAsText() << "'";
                ss << " '" << req_->getCaseSensitiveMethod();
                ss << " " << req_->getFullUrl() << " HTTP/1.1'";
                ss << " '" << status_ << "'";
                ss << " " << body_.size() * sizeof(char) << "B";
                ss << " '" << req_->getHeader("host") << "'";
                ss << " '" << req_->getHeader("user-agent") << "'";
                logger::trace(ss.str());
            }
        }

    private:
        uWS::HttpRequest* req_;
        uwsHttpResponse* res_;
        std::string_view status_ = uWS::HTTP_200_OK;
        std::string_view body_ = {};
        std::map<std::string_view, std::string_view> headers_;
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
        
        #define CREATE_HANDLER(handler_function) [&](auto* res, auto* req) { \
            delegate_request(res, req, [&](auto* rs, auto* rq) { handler_function(rs, rq); }); \
        }

        if(!is_single)
        {
            server.get(base_path, CREATE_HANDLER(handle_things));
            server.get(base_path + "/", CREATE_HANDLER(handle_things));
        }

        std::string thing_id_param = is_single ? "" : "/:thing_id";
        server.get(base_path + thing_id_param, CREATE_HANDLER(handle_thing));
        server.get(base_path + thing_id_param + "/", CREATE_HANDLER(handle_thing));
        server.get(base_path + thing_id_param + "/properties", CREATE_HANDLER(handle_properties));
        server.get(base_path + thing_id_param + "/properties/:property_name", CREATE_HANDLER(handle_property_get));
        server.put(base_path + thing_id_param + "/properties/:property_name", CREATE_HANDLER(handle_property_put));
        server.get(base_path + thing_id_param + "/actions", CREATE_HANDLER(handle_actions_get));
        server.post(base_path + thing_id_param + "/actions", CREATE_HANDLER(handle_actions_post));
        server.get(base_path + thing_id_param + "/actions/:action_name", CREATE_HANDLER(handle_actions_get));
        server.post(base_path + thing_id_param + "/actions/:action_name", CREATE_HANDLER(handle_actions_post));
        server.get(base_path + thing_id_param + "/actions/:action_name/:action_id", CREATE_HANDLER(handle_action_id_get));
        server.put(base_path + thing_id_param + "/actions/:action_name/:action_id", CREATE_HANDLER(handle_action_id_put));
        server.del(base_path + thing_id_param + "/actions/:action_name/:action_id", CREATE_HANDLER(handle_action_id_delete));
        server.get(base_path + thing_id_param + "/events", CREATE_HANDLER(handle_events));
        server.get(base_path + thing_id_param + "/events/:event_name", CREATE_HANDLER(handle_events));

        server.any("/*", CREATE_HANDLER(handle_invalid_requests));
        server.options("/*", CREATE_HANDLER(handle_options_requests));

        for(auto& thing : things.get_things())
        {
            auto thing_id = thing->get_id();
            uWebsocketsApp::WebSocketBehavior<std::string> ws_behavior;
            ws_behavior.compression = uWS::SHARED_COMPRESSOR;
            ws_behavior.open = [thing_id](auto *ws)
            {
                std::string* ws_id = (std::string *) ws->getUserData();
                ws_id->append(generate_uuid());

                logger::trace("websocket open " + *ws_id);
                ws->subscribe(thing_id + "/properties");
                ws->subscribe(thing_id + "/actions");
            };
            ws_behavior.message = [thing_id, thing](auto *ws, std::string_view message, uWS::OpCode op_code)
            {
                logger::trace("websocket msg " + *((std::string*)ws->getUserData()) + ": " + std::string(message));
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
                logger::trace("websocket close " + *((std::string*)ws->getUserData()));
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
        logger::info("Start WebThingServer v" + std::string(version) + " hosting '" + things.get_name() + 
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
            
            mdns_service = std::make_unique<MdnsService>();
            mdns_service->start_service(things.get_name(), "_webthing._tcp.local.", port, base_path + "/", is_ssl_enabled());

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

    void delegate_request(uwsHttpResponse* res, uWS::HttpRequest* req,
        std::function<void(uwsHttpResponse*, uWS::HttpRequest*)> handler)
    {
        // pre filter
        if(!validate_host(req))
        {
            Response response(req, res);
            response.forbidden().end();
            return;
        }

        // execute callback
        handler(res,req);
    }

    void handle_invalid_requests(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto host = req->getHeader("host");
        auto path = req->getUrl();

        if(path.back() == '/' && path != "/" && path != (base_path + "/"))
        {
            // redirect to non-trailing slash url
            auto location = (is_ssl_enabled() ? "https://" : "http://") + std::string(host) + std::string(path.data(), path.size()-1);
            response.header("Location", location.c_str());
            response.moved_permanently().end();
            return;
        }

        response.method_not_allowed().end();
    }

    void handle_options_requests(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);
        response.no_content().end();
    }

    json prepare_thing_description(Thing* thing, uWS::HttpRequest* req)
    {
        std::string http_protocol = is_ssl_enabled() ? "https" : "http";
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

    void handle_things(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        json descriptions = json::array();
        
        for(auto thing : things.get_things())
        {
            json desc = prepare_thing_description(thing, req);
            descriptions.push_back(desc);
        }
        
        response.json(descriptions.dump()).end();
    }

    void handle_thing(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            response.not_found().end();
            return;
        }

        json description = prepare_thing_description(*thing, req);

        response.json(description.dump()).end();
    }

    void handle_properties(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            response.not_found().end();
            return;
        }

        response.json((*thing)->get_properties().dump()).end();
    }

    void handle_property_get(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        auto property_name = find_property_name_from_url(req);

        if(!thing || !property_name)
        {
            response.not_found().end();
            return;
        }

        auto property = (*thing)->find_property(*property_name);

        if(!property)
        {
            response.not_found().end();
            return;
        }

        response.json(property->get_property_value_object().dump()).end();
    }

    void handle_property_put(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        auto property_name_in_url = find_property_name_from_url(req);

        if(!thing || !property_name_in_url)
        {
            response.not_found().end();
            return;
        }

        auto property = (*thing)->find_property(*property_name_in_url);

        if(!property)
        {
            response.not_found().end();
            return;
        }

        res->onData([res, req, thing, property_name_in_url, property](std::string_view body_chunk, bool is_last)
        {
            if(is_last)
            {
                Response response(req, res);

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

                    response.json(property->get_property_value_object().dump()).end();
                }
                catch(std::exception& ex)
                {
                    json body = {{"message", ex.what()}};
                    response.bad_request().json(body.dump()).end();
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
    void handle_actions_get(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            response.not_found().end();
            return;
        }

        // can be std::nullopt which results in a collection of all actions
        auto action_name = find_action_name_from_url(req);
        response.json((*thing)->get_action_descriptions(action_name).dump()).end();
    }


    // Handles POST requests to:
    // * /actions
    // * /actions/<action_name>
    void handle_actions_post(uwsHttpResponse* res, uWS::HttpRequest* req)
    {

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            Response(req, res).not_found().end();
            return;
        }

        auto action_name_in_url = find_action_name_from_url(req);

        res->onData([res, req, thing, action_name_in_url](std::string_view body_chunk, bool is_last)
        {
            if(is_last)
            {
                Response response(req, res);

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

                    json response_body = action->as_action_description();
                    std::thread action_runner([action]{
                        action->start();
                    });
                    action_runner.detach();

                    response.created().json(response_body.dump()).end();
                }
                catch(std::exception& ex)
                {
                    json body = {{"message", ex.what()}};
                    response.bad_request().json(body.dump()).end();
                }
            }
        });

        res->onAborted([]{
            logger::debug("transfer request body aborted");
        });

    }

    void handle_action_id_get(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        auto action_name = find_action_name_from_url(req);
        auto action_id = find_action_id_from_url(req);

        if(!thing || !action_name || !action_id)
        {
            response.not_found().end();
            return;
        }

        auto action = (*thing)->get_action(*action_name, *action_id);
        if(!action)
        {
            response.not_found().end();
            return;
        }

        response.json(action->as_action_description().dump()).end();
    }

    // TODO: this is not yet defined in the spec
    // also cf. https://webthings.io/api/#actionrequest-resource
    void handle_action_id_put(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            response.not_found().end();
            return;
        }

        response.end();
    }

    void handle_action_id_delete(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        auto action_name = find_action_name_from_url(req);
        auto action_id = find_action_id_from_url(req);

        if(!thing || !action_name || !action_id)
        {
            response.not_found().end();
            return;
        }

        auto action = (*thing)->get_action(*action_name, *action_id);
        if(!action)
        {
            response.not_found().end();
            return;
        }

        if(!(*thing)->remove_action(*action_name, *action_id))
        {
            response.not_found().end();
            return;
        }

        response.no_content().end();
    }

    // Handles requests to:
    // * /events
    // * /events/<event_name>
    void handle_events(uwsHttpResponse* res, uWS::HttpRequest* req)
    {
        Response response(req, res);

        auto thing = find_thing_from_url(req);
        if(!thing)
        {
            response.not_found().end();
            return;
        }

        // can be std::nullopt which results in a collection of all events
        auto event_name = find_event_name_from_url(req);
        response.json((*thing)->get_event_descriptions(event_name).dump()).end();
    }

    // forward thing messages to servers websocket clients
    void handle_thing_message(const std::string& topic, const json& message)
    {
        if(!webserver_loop)
            return;

        std::string t = topic;
        std::string m = message.dump();

        webserver_loop->defer([this, t, m]{
            logger::trace("server broadcast : " + t + " : " + m);
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