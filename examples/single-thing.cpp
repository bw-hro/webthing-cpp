// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include <bw/webthing/webthing.hpp>

using namespace bw::webthing;

struct FadeAction : public Action
{
    FadeAction(Thing* thing, std::optional<json> input)
        : Action(generate_uuid(), thing, this, "fade", input)
    {}

    void perform_action()
    {
        int duration = (*get_input())["duration"];
        std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        get_thing<Thing>()->set_property("brightness", (*get_input())["brightness"].get<int>());
        emit_event(get_thing<Thing>(), "overheated", 102);
    }
};

auto make_lamp()
{
    auto thing = make_thing("urn:dev:ops:my-lamp-1234", "My Lamp", std::vector<std::string>({"OnOffSwitch", "Light"}), "A web connected lamp");
    link_property(thing, "on", true, {
        {"@type", "OnOffProperty"},
        {"title", "On/Off"},
        {"type", "boolean"},
        {"description", "Whether the lamp is turned on"}});
       
    link_property(thing, "brightness", 50, {
        {"@type", "BrightnessProperty"},
        {"title", "Brightness"},
        {"type", "integer"},
        {"description", "The level of light from 0-100"},
        {"minimum", 0},
        {"maximum", 100},
        {"unit", "percent"}});

    link_action<FadeAction>(thing, "fade", {
        {"title", "Fade"},
        {"description", "Fade the lamp to a given level"},
        {"input", {{"type", "object"},
                    {"required", {"brightness", "duration"}},
                    {"properties", {{"brightness", {{"type", "integer"},
                                                    {"minimum", 0},
                                                    {"maximum", 100},
                                                    {"unit", "percent"}}},
                                    {"duration", {{"type", "integer"},
                                                    {"minimum", 1},
                                                    {"unit", "milliseconds"}}}}}}}});

    link_event(thing, "overheated", {
        {"description", "The lamp has exceeded its safe operating temperature"},
        {"type", "number"},
        {"unit", "degree celsius"}});

    return thing;
}

int main()
{
    auto lamp = make_lamp();
    WebThingServer::host(SingleThing(lamp.get())).port(8888).start();
    return 0;
}
