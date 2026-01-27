# gameserver

## Debug

```sh
# Run from project root directory
docker build --platform linux/amd64 -t debug-gameserver -f ./dockerfiles/gameserver.Dockerfile --target builder-debug .
```

```sh
# run from project root directory
docker run --rm -it -p 22222:80 \
-v $(pwd)/common/VXFramework:/project/common/VXFramework \
-v $(pwd)/common/VXGameServer:/project/common/VXGameServer \
-v $(pwd)/common/VXLuaSandbox:/project/common/VXLuaSandbox \
-v $(pwd)/common/VXNetworking:/project/common/VXNetworking \
-v $(pwd)/cubzh/deps/xptools:/project/cubzh/deps/xptools \
-v $(pwd)/servers/gameserver:/project/servers/gameserver \
-v $(pwd)/cubzh/bundle:/bundle \
-v $(pwd)/cubzh/lua/modules:/bundle/modules \
debug-gameserver bash
```

```sh
./compileExeDebug.sh
```

### with GDB

```sh
gdb gameserver

run dev 5be7825d-b412-4cbb-84bc-505d21f24614 xcode- https://api.cu.bzh:443 8
```


```sh
./gameserver dev 5be7825d-b412-4cbb-84bc-505d21f24614 xcode- https://api.cu.bzh:443 8
```

Compile

```sh
/project/servers/gameserver/compileExeDebug.sh
```

Run

```sh
/project/servers/gameserver/gameserver
# /project/common/VXGameServer/gameserver
```

