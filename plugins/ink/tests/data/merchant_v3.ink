// The ink Plugin's test story, third version: merchant.ink with the knot
// hub renamed stall, so a save taken in hub has no knot to move to.
VAR gold = 10
EXTERNAL give_item(item, count)
EXTERNAL play_sound(id)

-> stall

=== stall ===
Welcome to my stall. #speaker:merchant #line:hub_welcome
{~Fine weather today.|Cold wind tonight.|The road is quiet.} #speaker:merchant #line:hub_weather
Today's lucky number is {RANDOM(1, 100)}. #speaker:merchant #line:hub_lucky
+ [Buy a sword #line:hub_buy]
    ~ gold = gold - 5
    ~ give_item("sword", 2)
    Here you go. #speaker:merchant #line:hub_sold
    ~ play_sound(3)
    -> stall
+ [Haggle #line:hub_haggle]
    -> haggle ->
    -> stall
+ [Weigh the coins #line:hub_weigh]
    The scale reads {1.5} and {0.25}. #speaker:merchant #line:hub_scale
    -> stall
+ [Leave #line:hub_leave]
    ~ play_sound(7)
    Farewell. #speaker:merchant #line:hub_farewell #mood:warm
    -> END

=== haggle ===
You want a better price? #speaker:merchant #line:haggle_ask
+ [Insist #line:haggle_insist]
    No. #speaker:merchant #line:haggle_no
    ->->
