
# LightToSpread

Simplify agents that connect to spread.

Requires spread (well duh).

Consists of three utilities.  lightSink & lightSource and toSpread

lightSink recieves messages from spread and sends them to stdout.
lightSource does the opposite, i.e. takes data from stdin and send it to spread.
toSpread acts as a proxy.

No format or protocol is assumed for the messages 

## Examples of use


1. Modify bridge.json to suit your emvironment.
2. Copy start.rc o ~/.start.rc and modify.
2. run spuser
```bash
spuser -s 4803@<ip address
```
Then select j (for join) you will be prompted for a group name. Enter global, repeat this and enter mysql as the group.

2. Run make.
3. run ./spreadToMySQL
```
./spreadToMySQL -g mysql
```

You will see spreadToMySQL join on the spuser screen.

Then, on another screen

```bash
toSpread
```

