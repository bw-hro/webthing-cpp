// Webthing-CPP
// SPDX-FileCopyrightText: 2023-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#include "bw/webthing/webthing.hpp"
#include "gui-html.hpp"

using namespace bw::webthing;

struct SlotMachine
{
    typedef std::tuple<std::string, std::string, std::string> card_set;
    const card_set unknown_cards = {"?", "?", "?"};
    card_set current_cards = unknown_cards;

    int coins_inserted = 0;

    std::function<void (int)> credit_changed_callback;
    std::function<void (card_set)> cards_changed_callback;
    std::function<void (int)> win_callback;

    void on_credit_changed(std::function<void (int)> callback)
    {
        credit_changed_callback = callback;
    }

    void on_cards_changed(std::function<void (card_set)> callback)
    {
        cards_changed_callback = callback;
    }

    void on_win(std::function<void (int)> callback)
    {
        win_callback = callback;
    }

    void set_cards(const card_set& cards)
    {
        current_cards = cards;
        if(cards_changed_callback)
            cards_changed_callback(current_cards);
    }

    void insert_coin()
    {
        coins_inserted++;
        if(credit_changed_callback)
            credit_changed_callback(coins_inserted);
    }

    void consume_coin()
    {
        if(coins_inserted < 1)
            throw std::runtime_error("The credit is exhausted.");
                
        coins_inserted--;
        if(credit_changed_callback)
            credit_changed_callback(coins_inserted);
    }

    void next_round()
    {
        consume_coin();
        set_cards(unknown_cards);

        // some addtional delay
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto n_to_c = [](int n)
        {
            switch(n)
            {
                case 1:
                    return "♦";
                case 2:
                    return "♥";
                case 3:
                    return "♠";
                default:
                    return "♣";
            }
        };

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1, 4);
    
        auto c1 = n_to_c(dist(gen));
        auto c2 = n_to_c(dist(gen));
        auto c3 = n_to_c(dist(gen));
        card_set cards{c1, c2, c3};

        set_cards(cards);

        int win = calculate_win(cards);
        if(win > 0 && win_callback)
            win_callback(win);
    }

    int calculate_win(card_set cards) const
    {
        auto[c1, c2, c3] = cards;
        if(c1 == c2 && c2 == c3)
        {
            if(c1 == "♣")
                return 4;
            if(c1 == "♠")
                return 8;
            if(c1 == "♦")
                return 12;
            if(c1 == "♥")
                return 16;
        }
        return 0;
    }
};

static SlotMachine slot_machine;

struct InsertCoinAction : Action
{
    InsertCoinAction(Thing* thing)
        : Action(generate_uuid(), thing, this, "insert_coin", 1)
    {}

    void perform_action()
    {
        logger::info("insert_coin");
        slot_machine.insert_coin();
    }
};

struct PlayGameAction : Action
{
    PlayGameAction(Thing* thing)
        : Action(generate_uuid(), thing, this, "play_game")
    {
        if(slot_machine.coins_inserted < 1)
            throw std::runtime_error("At least 1 coin credit is required to play a game.");
    }

    void perform_action()
    {
        logger::info("play_game");
        slot_machine.next_round();
    }
};

json card_set_to_json(const SlotMachine::card_set& cards)
{
    auto[c1, c2, c3] = cards; 
    return json{{"c1",c1},{"c2",c2},{"c3",c3}};
}

auto make_gui_thing()
{
    auto thing = make_thing("urn:gui-thing-123", "The WebThing Slot Machine", "SLOT_MACHINE_THING", "A slot machine thing with GUI");
    thing->set_ui_href("/gui");

    link_property(thing, "coins", slot_machine.coins_inserted, {
        {"title", "coins"},
        {"type", "integer"},
        {"minimum", 0},
        {"unit", "Euro"},
        {"description", "Credit available."}});

    link_property(thing, "cards", card_set_to_json(slot_machine.current_cards), {
        {"title", "cards"},
        {"type", "object"},
        {"description", "The current card set consisting of 3 chars from [♠,♥,♦,♣,?]."}});

    link_action<PlayGameAction>(thing, "play_game", json::object({
        {"title", "Play a game"},
        {"description", "Play a game to win some coins. Make sure to insert one before :)"}
    }));

    link_action<InsertCoinAction>(thing, "insert_coin", json::object({
        {"title", "Insert coin"},
        {"description", "Please insert a coin to to increase the credit."},
        {"input", {
            {"type", "integer"},
            {"enum", {1}},
            {"unit", "Euro"}}}
    }));

    link_event(thing, "win", {
            {"description", "You win!"},
            {"type", "integer"},
            {"unit", "Euro"}});

    slot_machine.on_credit_changed([thing](int credit)
    {
        thing->set_property("coins", credit);
    });

    slot_machine.on_cards_changed([thing](auto cards)
    {
        thing->set_property("cards", card_set_to_json(cards));
    });

    slot_machine.on_win([thing](int amount)
    {
        emit_event(thing, "win", amount);
    }); 

    return thing;
}

int main()
{
    // configure logger
    logger::set_level(log_level::trace);
    logger::use_color(true);

    // setup thing
    auto thing = make_gui_thing();
    auto server = WebThingServer::host(SingleThing(thing.get()))
        .base_path("/wt-api")
        .port(8888)
        .build();

    auto web = server.get_web_server();
    
    // register additional html page
    web->get("/gui", [&](auto res, auto req){
        WebThingServer::Response(req, res).html(gui_html).end();
    });

    // register additional static file
    web->get("/favicon.ico", [&](auto res, auto req){
        WebThingServer::Response(req, res)
            .header("Content-Type","image/svg+xml")
            .body(R"(<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16"><rect width="16" height="16" fill="white"/><circle cx="8" cy="8" r="5" fill="black"/></svg>)")
            .end();
    });

    // thing description will be available at: http://localhost:8888/wt-api
    // GUI consuming the thing will be available at: http://localhost:8888/gui
    server.start();
    return 0;
}