# Stateful multi-message crash example (TCP + UDP)

A minimal FTP-like server (`server.c`) whose crash is reachable **only** after
the ordered three-message sequence:

```
USER <x>\r\n   ->   PASS <y>\r\n   ->   STOR <long>\r\n
```

## Run

```sh
./run.sh tcp        # or: ./run.sh udp   [seconds]
```
