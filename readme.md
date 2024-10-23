[project_name]: Webthing-CPP

# Webthing-CPP

[![CI Ubuntu](https://github.com/bw-hro/webthing-cpp/actions/workflows/ubuntu.yml/badge.svg?branch=master)](https://github.com/bw-hro/webthing-cpp/actions/workflows/ubuntu.yml)
[![CI Windows](https://github.com/bw-hro/webthing-cpp/actions/workflows/windows.yml/badge.svg?branch=master)](https://github.com/bw-hro/webthing-cpp/actions/workflows/windows.yml)
[![CI macOS](https://github.com/bw-hro/webthing-cpp/actions/workflows/macos.yml/badge.svg?branch=master)](https://github.com/bw-hro/webthing-cpp/actions/workflows/macos.yml)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/bw-hro/webthing-cpp/master/LICENSE.txt)
[![GitHub Releases](https://img.shields.io/github/release/bw-hro/webthing-cpp.svg)](https://github.com/bw-hro/webthing-cpp/releases)
[![Vcpkg Version](https://img.shields.io/vcpkg/v/webthing-cpp)](https://vcpkg.link/ports/webthing-cpp)

Webthing-CPP is a modern CPP/C++17 implementation of the [WebThings API](https://webthings.io/api). Goal of the project is to offer an easy way to set up digital twins with web interface for arbitrary things by just specifying their properties, actions and events. This projects focus lies on an easy to use API heavily inspired by similar projects from the [Java](https://github.com/WebThingsIO/webthing-java) and [Python](https://github.com/WebThingsIO/webthing-python) world.

Webthing-CPP comes with MIT license without any warranty. DISCLAIMER: At the moment this project is in an early stage. Please make sure to perform sufficient number of tests if it suits your needs regarding stability before using it in production.

## Project structure

This project follows a header only approach to make integration into own projects easier. Nevertheless it relies on some dependencies to implement its features:

- [µWebSockets](https://github.com/uNetworking/uWebSockets) is used as backing http/websocket server.
- [nlohmann::json](https://github.com/nlohmann/json) and [json-schema-validator](https://github.com/pboettch/json-schema-validator) are used to make working with json more comfortable.
- [mdns](https://github.com/mjansson/mdns) is used for easy service discovery.
- [OpenSSL](https://github.com/openssl/openssl) is used when SSL support is required. 

The projects sources can be found in the _include_ folder. Beside the library sources the project is shipped with some examples for demonstration purposes located in the _examples_ folder. In addition some unit tests backed by [Catch2](https://github.com/catchorg/Catch2) framework can be found in _test_ folder. In _tools_ folder there are some helpers available related to _vcpkg_, tests and certificates.  

## Defines 

__WT_USE_JSON_SCHEMA_VALIDATION__  

When ```WT_USE_JSON_SCHEMA_VALIDATION``` is defined Webthing-CPP will validate JSON input for thing properties and thing actions against JSON scheme defined by the thing description. Otherwise validation will be omitted.  

__WT_UNORDERED_JSON_OBJECT_LAYOUT__  

Webthing-CPP uses ```nlohmann::ordered_json``` as default json implementation. This specialization maintains the insertion order of object keys. By defining ```WT_UNORDERED_JSON_OBJECT_LAYOUT``` Webthing-CPP will use ```nlohmann::json``` as json implementation.

__WT_WITH_SSL__  

By defining ```WT_WITH_SSL``` Webthing-CPP will use the ```uWS::SSLApp``` as backing webserver. When definition is missing it will use ```uWS::App```.

## Build system

Webthing-CPP uses _cmake_ in conjunction with _vcpkg_ as default build system. By default, the build system is configured to statically link all dependencies to build simple self-contained executables.

The __build.sh__ script is a little helper to ensure cmake is called with correct parameters and vcpkg installs all required dependencies. For Windows users there is an alternative __build.bat__ script available. Following arguments are supported:

__clean__  

Deletes the build folder that cmake creates to ensure a fresh rebuild.

__release__  

Use _Release_ as cmake build type. _Debug_ will be used as default.

__with_ssl__  

Configures the project to support SSL for WebThingServer and installs additional required dependencies.

__win32__

Windows only: Use _Win32_ as target architecture. _x64_ will be used as default.

## SSL support

Build project with SSL support.

```sh
./build.sh clean release with_ssl
```

A self signed certificate for test purposes can be created by using the __create-pems.sh__ script from the __tools__ folder. This will create a _key.pem_ as well as a _cert.pem_. Make sure to configure the ```WebThingServer``` with correct ```SSLOptions``` e.g.:

```C++
SSLOptions ssl_options;
ssl_options.key_file_name = "key.pem";
ssl_options.cert_file_name = "cert.pem";
ssl_options.passphrase = "1234";

auto server = WebThingServer::host(things)
    .port(8888)
    .ssl_options(ssl_options)
    .build();
```

## Examples

At the moment three example applications are available.

- [single-thing.cpp](examples/single-thing.cpp) Shows how to set up a simple WebThing with two properties and an action.
- [multiple-thing.cpp](examples/multiple-things.cpp) Shows how to host more then one WebThing in a single application. Things are a fake light and a fake humidity sensor.
- [gui-thing.cpp](examples/gui-thing.cpp) Demonstrates how to embed  a HTML GUI of a fake slot machine and  how interact with it.

### Example implementation

In this code-walkthrough we will set up a dimmable light and a humidity sensor (both using fake data, of course). All working examples can be found in the [examples](/examples/) directory.

#### Dimmable light

Imagine you have a dimmable light that you want to expose via the WebThings API. The light can be turned on/off and the brightness can be set from 0% to 100%. Besides the name, description, and type, a _Light_ is required to expose two properties:

* ```on```: the state of the light, whether it is turned on or off
  - Setting this property via a ```PUT {"on": true/false}``` call to the REST API toggles the light.

* ```brightness```: the brightness level of the light from 0-100%
  - Setting this property via a PUT call to the REST API sets the brightness level of this light.

First we create a new Thing:

```C++
auto thing = make_thing("urn:dev:ops:my-lamp-1234", "My Lamp",
    std::vector<std::string>({"OnOffSwitch", "Light"}),
    "A web connected lamp");
```
Now we can add the required properties.

The ```on``` property reports and sets the on/off state of the light. For this, we need to have a ```Value``` object which holds the actual state and also a method to turn the light on/off. For our purposes, we just want to log the new state if the light is switched on/off.

```C++
auto on_value = make_value(true, [](auto v){
        logger::info("On-State is now " + std::string( v ? "on" : "off"));
});
link_property(thing, "on", on_value, {
    {"@type", "OnOffProperty"},
    {"title", "On/Off"},
    {"type", "boolean"},
    {"description", "Whether the lamp is turned on"}});
```

The ``brightness`` property reports the brightness level of the light and sets the level. Like before, instead of actually setting the level of a light, we just log the level.

```C++
auto brightness_value = make_value(50, [](auto v){
    logger::info("Brightness is now " + std::to_string(v));
});
link_property(thing, "brightness", brightness_value, {
    {"@type", "BrightnessProperty"},
    {"title", "Brightness"},
    {"type", "integer"},
    {"description", "The level of light from 0-100"},
    {"minimum", 0},
    {"maximum", 100},
    {"unit", "percent"}});
```

Now we can add our newly created thing to the server and start it:

```C++
// If adding more than one thing, use MultipleThings() with a name.
// In the single thing case, the thing's name will be broadcast.
WebThingServer::host(SingleThing(thing.get())).port(8888).start();
```

This will start the server, making the light available via the WebThings REST API and announcing it as a discoverable resource on your local network via mDNS.

#### Sensor

Let's now also connect a humidity sensor to the server we set up for our light.

A _MultiLevelSensor_ (a sensor that returns a level instead of just on/off) has one required property (besides the name, type, and optional description): ```level```. We want to monitor this property and get notified if the value changes.

First we create a new Thing with a ```level``` property (Here we specify the property within the constructor of our custom Thing object):

* ``level``: tells us what the sensor is actually reading

  - Contrary to the light, the value cannot be set via an API call, as it wouldn't make much sense, to SET what a sensor is reading. Therefore, we are creating a **readOnly** property.

```C++
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
    }

    std::shared_ptr<Value<double>> level;
};
```

Now we have a sensor that constantly reports 0%. To make it usable, we need a thread or some kind of input when the sensor has a new reading available. For this purpose we start a thread that queries the physical sensor every few seconds. For our purposes, it just calls a fake method.

```C++
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
        std::thread([this]{
            while(true)
            {
                std::this_thread::sleep_for(std::chrono::seconds(3));

                // Update the underlying value, which in turn notifies
                // all listeners
                level->notify_of_external_update(read_from_gpio());
            }
        }).detach();
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
};
```

This will update our ```Value``` object with the sensor readings via the ```level->notify_of_external_update(read_from_gpio())``` call. The ```Value``` object now notifies the property and the thing that the value has changed, which in turn notifies all websocket listeners.

## Adding to Gateway

To add your web thing to the WebThings Gateway, install the "Web Thing" add-on and follow the instructions [here](https://github.com/WebThingsIO/thing-url-adapter#readme).
