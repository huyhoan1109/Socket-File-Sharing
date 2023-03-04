

## Using ngrok for hosting

Ngrok's website: [link](https://ngrok.com/)

### To install on Ubuntu
```
sudo snap install ngrok
```

### To host ngrok
```
ngrok tcp [port]
```
* `port` must be the same as server's local port
* Example result: `tcp://0.tcp.ap.ngrok.io:11091`
  * Use [this website](https://whatismyipaddress.com/hostname-ip), input `0.tcp.ap.ngrok.io` to get `IP Address`
  * `port` is 11091

## Run

After running ngrok 

Both client and server side must contain 'socket' directory 
