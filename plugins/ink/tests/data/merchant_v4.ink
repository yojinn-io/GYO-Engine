// The ink Plugin's test story, fourth version: merchant.ink without the
// haggle and weigh choices, so a save taken in one of them has no place to move to.
VAR gold = 10
EXTERNAL give_item(item, count)
EXTERNAL play_sound(id)

-> hub

=== hub ===
Welcome to my stall. #speaker:merchant #line:hub_welcome
{~Fine weather today.|Cold wind tonight.|The road is quiet.} #speaker:merchant #line:hub_weather
Today's lucky number is {RANDOM(1, 100)}. #speaker:merchant #line:hub_lucky
+ [Buy a sword #line:hub_buy]
    ~ gold = gold - 5
    ~ give_item("sword", 2)
    Here you go. #speaker:merchant #line:hub_sold
    ~ play_sound(3)
    -> hub
+ [Leave #line:hub_leave]
    ~ play_sound(7)
    Farewell. #speaker:merchant #line:hub_farewell #mood:warm
    -> END

=== haggle ===
You want a better price? #speaker:merchant #line:haggle_ask
+ [Insist #line:haggle_insist]
    No. #speaker:merchant #line:haggle_no
    ->->
