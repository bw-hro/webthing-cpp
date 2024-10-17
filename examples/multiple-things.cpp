// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <csignal>
#include <filesystem>
#include <bw/webthing/webthing.hpp>

using namespace bw::webthing;

struct ExampleDimmableLight : public Thing
{
    struct OverheatedEvent : public Event
    {
        OverheatedEvent(Thing* thing, double temperature)
            : Event(thing, "overheated", temperature)
        {
            logger::warn("Overheated " + std::to_string(temperature));
        }
    };

    struct FadeAction : public Action
    {
        FadeAction(Thing* thing, json fade_input)
            : Action(generate_uuid(), thing, this, "fade", fade_input)
            , cancel(false)
        {
            logger::info("Fade to " + fade_input["brightness"].dump() + " in " + 
                fade_input["duration"].dump() + "ms");
        }

        bool cancel;

        void perform_action()
        {
            int duration = (*get_input())["duration"];
            int destination = (*get_input())["brightness"];

            bool interpolate = (*get_input()).value("interpolate", false); 
            if(!interpolate)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(duration));
                get_thing<Thing>()->set_property("brightness", destination);
            }
            else
            {
                int current = *get_thing<Thing>()->get_property<int>("brightness");
                int steps = 1 + std::ceil(std::abs(destination - current) / 10);
                int inc = 1 + (destination - current) / steps;
                int sleep = std::ceil(duration / steps);

                while(!cancel && current != destination)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
                    if(cancel)
                        return;

                    int incremented = current + inc;
                    int next_step = inc > 0 ? (incremented < destination ? incremented : destination)
                                            : (incremented > destination ? incremented : destination);
                    get_thing<Thing>()->set_property("brightness", next_step);
                    current = *get_thing<Thing>()->get_property<int>("brightness");
                }
            }

            emit_event(get_thing<Thing>(), OverheatedEvent(get_thing<Thing>(), 102));
        }

        void cancel_action()
        {
            logger::info("Cancel fade instance " + get_id());
            cancel = true;
        }
    };

    ExampleDimmableLight() : Thing(
        "urn:dev:ops:my-lamp-1234", "My Lamp", std::vector<std::string>{"OnOffSwitch", "Light"},
        "A web connected lamp")
    {
        auto on_value = make_value(true, [](auto v){
                logger::info("On-State is now " + std::string( v ? "on" : "off"));
        });
        link_property(this, "on", on_value, {
            {"@type", "OnOffProperty"},
            {"title", "On/Off"},
            {"type", "boolean"},
            {"description", "Whether the lamp is turned on"}});
        
        auto brightness_value = make_value(50, [](auto v){
            logger::info("Brightness is now " + std::to_string(v));
        });
        link_property(this, "brightness", brightness_value, {
            {"@type", "BrightnessProperty"},
            {"title", "Brightness"},
            {"type", "integer"},
            {"description", "The level of light from 0-100"},
            {"minimum", 0},
            {"maximum", 100},
            {"unit", "percent"}});

        link_action<FadeAction>(this, "fade", {
            {"title", "Fade"},
            {"description", "Fade the lamp to a given level"},
            {"input", {
                {"type", "object"},
                {"required", {"brightness", "duration"}},
                {"properties", {
                    {"brightness", {
                        {"type", "integer"},
                        {"minimum", 0},
                        {"maximum", 100},
                        {"unit", "percent"}}},
                    {"duration", {
                        {"type", "integer"},
                        {"minimum", 1},
                        {"unit", "milliseconds"}}},
                    {"interpolate", {
                        {"type", "boolean"},
                        {"default", false}}}}}}}});
      
        link_event(this, "overheated", {
            {"description", "The lamp has exceeded its safe operating temperature"},
            {"type", "number"},
            {"unit", "degree celsius"}});
    }

};

struct FakeGpioHumiditySensor : public Thing
{
    FakeGpioHumiditySensor() : Thing(
        "urn:dev:ops:my-humidity-sensor-1234", "My Humidity Sensor",
        "MultiLevelSensor", "A web connected humidity sensor")
    {
        level = make_value(0.0);
        link_property(this, "level", level,  {
            {"@type", "LevelProperty"},
            {"title", "Humidity"},
            {"type", "number"},
            {"description", "The current humidity in %"},
            {"minimum", 0},
            {"maximum", 100},
            {"unit", "percent"},
            {"readOnly", true}});

        // Start a thread that polls the sensor reading every 3 seconds
        runner = std::thread([this]{
            while(read_from_sensor)
            {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if(!read_from_sensor)
                    return;

                // Update the underlying value, which in turn notifies
                // all listeners
                double new_level = read_from_gpio();
                logger::info("setting new humidity level: " + std::to_string(new_level));
                level->notify_of_external_update(new_level);
            }
        });
    }

    void cancel()
    {
        logger::info("canceling the sensor");
        read_from_sensor = false;
        runner.join();
    }

    // Mimic an actual sensor updating its reading every couple seconds.
    double read_from_gpio()
    {
        std::random_device rd; 
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        return dist(gen);
    }

    std::shared_ptr<Value<double>> level;
    std::thread runner;
    bool read_from_sensor = true;
};

void run_server()
{
    // Create a thing that represents a dimmable light
    static ExampleDimmableLight light;
    
    // Create a thing that represents a humidity sensor
    static FakeGpioHumiditySensor sensor;

    MultipleThings multiple_things({&light, &sensor}, "LightAndTempDevice");

    try
    {
        // If adding more than one thing, use MultipleThings() with a name.
        // In the single thing case, the thing's name will be broadcast.
        static auto server = WebThingServer::host(multiple_things).port(8888).build();

        std::signal(SIGINT, [](int signal) {
            if (signal == SIGINT)
            {
                sensor.cancel();
                server.stop();
            }
        });
        server.start();

    }
    catch(std::exception& ex)
    {
        logger::error(std::string("exception when running server: ") + ex.what());
    }
}

int main()
{
    logger::set_level(log_level::info);
    run_server();
    return 0;
}