# Bomberman


server/server.c handles this:

start server
bind/listen
accept new TCP connection
perform HELLO/WELCOME
create client thread

server/client.c handles this:

for one connected client:
read messages forever
handle LEAVE / READY / MOVE / PING
broadcast updates
enqueue game events
