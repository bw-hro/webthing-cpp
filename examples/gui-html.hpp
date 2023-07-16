// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

constexpr char gui_html[] = R"""(
<!doctype html>
<html lang="en">
    <head>
        <title>Slot Machine GUI</title>
        <meta charset="UTF-8">
        <link rel="shortcut icon" href="data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAAXNSR0IArs4c6QAAAh5JREFUWEftlU2oaVEUx/9C5KsMTI2YGSBJpNSdKsyMDRlQUkrkY2bCTMqYESmlTCQT3xlQpkYGJkoykI/XPuX2rvve3ecct/t6ZY1OnbX+67f/e+29Bbfb7YZ/GIIXwMuB/9qBw+EAhULx1BnifQrO5zPsdjuq1Sp0Oh1vCN4AuVwOqVQKLpcL3W4XAoGAFwQvgPF4DIfDAeICiUKhgEgk8jMAu90OZrMZ6/X6vaFEIsFwOITRaOQMwckBcmt7PB60Wq1PjfR6PabTKVQqFScITgCZTAbpdPqvDXw+H+r1Oqd5YA1AVk1WT3u7kskkstksaxdYAaxWK9hsNuz3e1bCjUYDxA02QQUgk24ymbBcLtnoMTlkDhaLBbRaLbWGClAqlRAMBqlCjwl+vx+1Wo1aRwUg1o9GI6rQY4JUKsV2u4VSqfyylgogl8txPB45A5CCwWDAzM5XQQUQi8XvNx5Xin6/D6fT+RyAxWLBbDb7JOL1enE6nfD29oZoNPrpv0gkwmazgUajeQ6g3W7D7XZ/OP9kyufzOcLhMJrNJrNKYvfvQf4Vi0WqadQtIArkyU0kEsz9bzAYUC6XmaeYuNPpdHC5XBAIBJhvmUyGUCgE8loKhcLvAbirkEZ30ev1CrVazTS9Dxq5M4j1XIKVA38SnEwmsFqtyOfziMViXHp+yOUNEI/HUalUmC3p9Xo/D8C740MhbwdeAC8HvsuBX8VZLbA+wPbsAAAAAElFTkSuQmCC" />
        <script>
            var coins_in_slot_machine = 0;
            var required_subscriptions;
            var webthing_ws = null;
        
            function eval_thing_description(td)
            {
                if(typeof td['title'] === 'string')
                {
                    document.getElementsByTagName('h1')[0].innerHTML = td['title'];
                    document.title = td['title'];
                }

                required_subscriptions = Object.keys(td['events']);
                for(const link of td['links'])
                {
                    if(link.rel === 'alternate' && link.href.startsWith('ws'))
                    {
                        console.log("found websocket link:", link);
                        open_websocket(link.href, 1000, 1000, 2);
                    }
                }
            }

            function open_websocket(ws_url, wait_timer, wait_seed, multiplier)
            {

                let ws = new WebSocket(ws_url);
                console.log(`trying to connect to: ${ws.url}`);

                ws.onopen = () => {
                    console.log(`connection open to: ${ws.url}`);
                    wait_timer = wait_seed; //reset the wait_timer if the connection is made

                    on_thing_ws_open(ws);
                    ws.onclose = () => {
                        console.log(`connection closed to: ${ws.url}`);
                        show_message('Lost connection to SlotMachine...');
                        open_websocket(ws.url, wait_timer, wait_seed, multiplier);
                    };
                    
                    ws.onmessage = (msg) => on_thing_message(JSON.parse(msg.data));
                };
                
                ws.onerror = () => {
                    //increaese the wait timer if not connected, but stop at a max of 2n-1 the check time
                    if(wait_timer < 10000) wait_timer = wait_timer * multiplier; 
                    console.log(`error opening connection ${ws.url}, next attemp in : ${wait_timer/1000} seconds`);
                    show_message('Lost connection to SlotMachine...');
                    setTimeout(()=>{open_websocket(ws.url, wait_timer, wait_seed, multiplier)}, wait_timer);
                }
            }

            function send_message(msg)
            {
                console.log('send:', msg);
                webthing_ws.send(JSON.stringify(msg));
            }

            function on_thing_ws_open(ws)
            {
                webthing_ws = ws;

                let subscribe = {'messageType':'addEventSubscription', 'data':{}};
                for(const rs of required_subscriptions)
                    subscribe.data[rs] = {};

                send_message(subscribe);
            }

            function on_thing_message(msg)
            {
                console.log(msg);

                if(msg.messageType === 'propertyStatus' && 'cards' in msg.data)
                {
                    set_cards(msg.data.cards);
                    if(msg.data.cards.c1 !== '?')
                        check_user_wallet();
                }
                if(msg.messageType === 'propertyStatus' && 'coins' in msg.data)
                    set_coins(msg.data.coins);
                if(msg.messageType === 'event' && 'win' in msg.data)
                    on_win(msg.data.win.data);
            }

            function on_win(amount)
            {
                console.log('win ', amount);
                show_message('Congratulation! You win ' + amount + '€ !!!');
                for(let i = amount; i > 0; i--)
                    add_coin_to_user_wallet();
            }

            function set_cards(cards)
            {
                console.log('set_cards:', cards);
                set_card('c1', cards.c1);
                set_card('c2', cards.c2);
                set_card('c3', cards.c3);
            }

            function set_card(id, card)
            {
                let element = document.getElementsByClassName(id)[0];
                element.innerHTML = card === '?' ? '█' : card;
                element.classList.remove('black','red');
                if(card === '♥' || card === '♦')
                    element.classList.add('red');
                if(card === '♠' || card === '♣')
                    element.classList.add('black');
            }

            function set_coins(coins)
            {
                console.log('set_coins:', coins);
                coins_in_slot_machine = coins;
            }

            function play_game()
            {
                console.log('play_game');
                let play = {'messageType':'requestAction','data': {'play_game': {}}};
                send_message(play);
            }
            
            function insert_coin()
            {
                console.log('insert_coin');
                let insert_coin = {'messageType':'requestAction','data': {'insert_coin': {'input': 1}}};
                send_message(insert_coin);
            }

            function insert_coin_and_play_game(coin)
            {
                insert_coin();
                let coin_prom = new Promise(resolve => {
                    const interval_id = setInterval(() => {
                        if (coins_in_slot_machine > 0) {
                            clearInterval(interval_id);
                            resolve();
                        }
                    }, 100); // Check every 100ms
                });
                coin_prom.then(() => {
                    coin.remove();
                    play_game();
                });
            }

            function add_coin_to_user_wallet()
            {
                console.log('add 1€ to walet')

                let coin = document.createElement('button');
                coin.id = Math.random().toString(36).substring(2, 15);
                coin.draggable = true;
                coin.classList.add('euro1');
                coin.ondragstart = drag;
                coin.innerHTML = '1€';

                document.getElementsByClassName('wallet')[0].appendChild(coin);
            }

            function check_user_wallet()
            {
                let coin = document.querySelector('.wallet .euro1');
                if(coin === null)
                {
                    setTimeout(() => {
                        let coin = document.querySelector('.wallet .euro1');
                        if(coins_in_slot_machine === 0 && coin === null)
                            show_message("You lost all your money...<br/>Try 'ctrl' + 'space' :)");
                    }, 500);
                }
            }

            function allow_drop(ev)
            {
                ev.preventDefault();
            }
            
            function drag(ev)
            {
                ev.dataTransfer.setData('coin_id', ev.target.id);
            }

            function drop(ev)
            {
                ev.preventDefault();
                let data = ev.dataTransfer.getData('coin_id');
                let coin = document.getElementById(data);
                ev.target.appendChild(coin);
                insert_coin_and_play_game(coin);
            }

            function show_message(message)
            {
                var m = document.getElementsByClassName('message')[0];
                m.innerHTML = message;
                m.style.display = 'block';
                m.addEventListener('click', e => {
                    m.style.display = 'none';
                });
            }

            document.addEventListener('DOMContentLoaded', e => {
                for(let i = 0; i < 10; i++)
                    add_coin_to_user_wallet();
                    
                console.log('Welcome to SlotMachine example');
                var td_url = window.location.origin;
                if(td_url === null || !td_url.startsWith('http'))
                    td_url = 'http://localhost:8888/';

                fetch(td_url)
                    .then((response) => response.json())
                    .then(td => eval_thing_description(td));
            });

            document.addEventListener('keydown', ev => {
                if(ev.code.toLowerCase() === 'space')
                {
                    var m = document.getElementsByClassName('message')[0];
                    var msg_visible = m.style.display === 'block';
                    if(msg_visible)
                        m.style.display = 'none';

                    if(ev.ctrlKey)
                    {
                        add_coin_to_user_wallet();
                        return;
                    }

                    if(msg_visible)
                        return;

                    let coin = document.querySelector('.wallet .euro1');
                    if(coin !== null)
                    {
                        insert_coin_and_play_game(coin);
                    }
                }
            });
        </script>
        <style>
            *{
                margin: 0;
                padding: 0;
            }
            body {
                margin: 0 auto;
                max-width: 760px;
                font-family: monospace;
            }
            h1, .cards, .cards span, .manual, .wallet{
                border: 2px solid darkgray;
            } 
            h1, .cards, .manual{
                border-bottom: none;
            }
            .wallet {
                background: #383838;
                padding: 2em;
                height: 11em;
                box-shadow: inset 0 0 40px black;
                border-radius: 2em;
                position: relative;
                overflow: auto;
            }
            .manual {
                display: flex;
                background: #eee;
                margin-bottom: -2.5em;
            }
            .slot {
                padding: 1em;
                flex: 2;
                font-size: 2em;
                line-height: 2em;
                text-align: center;
                box-shadow: inset 0 0 1em silver;
                cursor: pointer;
                padding-bottom: 2em;
            }
            .chances {
                flex: 1;
                padding: 2em;
                border-right: 2px solid darkgray;
                font-size: 1.2em;
                line-height: 1.5em;
            }
            .cards {
                text-align: center;
                border-radius: 2em 2em 0 0;
                padding: 2em;
            }
            h1 {
                padding: 1em;
                text-align: center;
                margin: 3em 5em 0;
                border-radius: 1em 1em 0 0;
                background: #eee;
            }
            .cards span {
                display: inline-block;
                font-size: 12em;
                padding: 40px;
                margin: 20px;
                color: darkgray;
                border-radius: 10px;
                box-shadow: 5px 5px 5px silver;
            }
            .black {
                color: black !important;
                border-color: black !important;
            }
            .red {
                color: red !important;
                border-color: red !important;
            }
            button {
                cursor: pointer;
            }
            .euro1 {
                width: 4em;
                height: 4em;
                background: #eee;
                border: 6px solid #ffd70080;
                border-radius: 2em;
                box-shadow: 2px 2px 2px silver;
                opacity: .999;
                margin: .5em;
            }
            @keyframes blinker {
                25% {
                    background: #ff0000b8;
                }
             
                75% {
                    background: rgba(0, 0, 0, 0.654);
                }
            }
            .message {
                display: none;
                font-size: 2em;
                text-shadow: 0px 0px 3px red;
                color: #ffffff;
                background: #ff0000b8;
                text-align: center;
                width: 650px;
                padding: 1em;
                opacity: .9;
                position: fixed;
                top: 289px;
                left: 50%;
                transform: translateX(-50%);
                cursor: pointer;
                animation: blinker 1s step-start infinite;
            }
        </style>
    </head>
    <body>
        <div class="message"></div>
        <div class="machine">
            <h1>Slot Machine</h1>
            <div class="cards">
                <span class="c1">█</span>
                <span class="c2">█</span>
                <span class="c3">█</span>
            </div>
            <div class="manual">
                <div class="chances">
                    <p>Flush of <span class="red">♥</span> ... 16€</p>
                    <p>Flush of <span class="red">♦</span> ... 12€</p>
                    <p>Flush of ♠ .... 8€</p>
                    <p>Flush of ♣ .... 4€</p>
                </div>
                <div class="slot" ondrop="drop(event)" ondragover="allow_drop(event)">
                    Drop a coin here or hit<br/> 'space' to start a game
                </div>
            </div>
            <div class="wallet">
            </div>
        </div>
    </body>
</html>
)""";