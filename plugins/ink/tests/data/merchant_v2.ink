// The ink Plugin's test story, second version: the migration target of
// merchant.ink, with one more global, one more line and one changed line,
// under the same knot names. Every line and choice carries a
// line id (#line:<id>); a speaker's lines carry #speaker:<id>.
VAR gold = 10
VAR reputation = 3
EXTERNAL give_item(item, count)
EXTERNAL play_sound(id)

-> hub

=== hub ===
Welcome to my stall. #speaker:merchant #line:hub_welcome
You carry {gold} gold, reputation {reputation}. #speaker:merchant #line:hub_purse
{~Fine weather today.|Cold wind tonight.|The road is quiet.} #speaker:merchant #line:hub_weather
Today's lucky number is {RANDOM(1, 100)}. #speaker:merchant #line:hub_lucky
+ [Buy a sword #line:hub_buy]
    ~ gold = gold - 5
    ~ give_item("sword", 2)
    Here you go, friend. #speaker:merchant #line:hub_sold
    ~ play_sound(3)
    -> hub
+ [Haggle #line:hub_haggle]
    -> haggle ->
    -> hub
+ [Weigh the coins #line:hub_weigh]
    The scale reads {1.5} and {0.25}. #speaker:merchant #line:hub_scale
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
