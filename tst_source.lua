#!/usr/bin/lua

toClientName   = "/var/tmp/fred.0"
fromClientName = "/var/tmp/fred.1"

print("One")
toClient   = io.open( toClientName, "w")
print(toClient)

os.execute( "sleep 1")
print("Two")
fromClient   = io.open( fromClientName, "r")
print(fromClient)

print("Three")
toClient:write("^connect\n")
toClient:flush()

os.execute( "sleep 1")

a=1

while a < 10 do
    print "Writing"
--    print(fromClient:read("*line"))
    msg = "Data:" .. a .. "\n"
    print(msg)
    toClient:write(msg)
    toClient:flush()
    a=a+1
    os.execute( "sleep 5")
end
-- os.execute( "sleep 10")
