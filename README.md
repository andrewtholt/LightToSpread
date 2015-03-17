
LightToSpread

Simplify agents that connect to spread.

Requires spread (well duh).

Consists of two utilities.  lightSink & lightSource and toSpread

lightSink recieves messages from spread and sends them to stdout.
lightSource does the opposite, i.e. takes data from stdin and send it to spread.
toSpread acts as a proxy.

No format or protocol is assumed for the messages 
