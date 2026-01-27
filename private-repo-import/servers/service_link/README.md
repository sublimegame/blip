# Service: link

This service runs on machine `services-us-1`.

## Requirements

### Install the config storage file on the machine

Config storage file for `link` service is stored on the machine and is mounted inside the container (read-write).

**Paths**

- on the machine: `/voxowl/link/config.json`
- in the container: `/voxowl/link/config.json`

**Example of file content**

```json
{"Redirects":{"cubzhdiscord":{"hash":"cubzhdiscord","dest":"https://discord.gg/ytS7u5UcQa","creator":"gdevillele","updater":"gdevillele","created":"2023-11-07T09:17:44.832174436Z","updated":"2023-11-07T09:17:44.83217457Z"},"decka31d5bf1":{"hash":"decka31d5bf1","dest":"https://slides.com/cubzh/cubzh/fullscreen?token=nxFaLcXr","creator":"aduermael","updater":"aduermael","created":"2024-03-12T17:35:39.81151296Z","updated":"2024-03-12T17:35:39.811513027Z"},"discord":{"hash":"discord","dest":"https://discord.com/invite/UHCzKpK3Ym","creator":"gdevillele","updater":"gdevillele","created":"2024-01-31T10:19:25.61047181Z","updated":"2024-01-31T10:19:25.610471883Z"},"fromparticubes":{"hash":"fromparticubes","dest":"https://cu.bzh/","creator":"aduermael","updater":"aduermael","created":"2023-11-20T21:31:53.008001725Z","updated":"2023-11-20T21:31:53.00800179Z"},"googleplayinternal":{"hash":"googleplayinternal","dest":"https://play.google.com/apps/internaltest/4699389714564235618","creator":"gdevillele","updater":"gdevillele","created":"2024-04-09T09:01:07.490488149Z","updated":"2024-04-09T09:01:07.490488216Z"},"test1":{"hash":"test1","dest":"https://cu.bzh","creator":"gdevillele","updater":"gdevillele","created":"2023-11-06T09:20:59.033083499Z","updated":"2023-11-06T09:20:59.033083572Z"},"test2":{"hash":"test2","dest":"https://cu.bzh","creator":"gdevillele","updater":"gdevillele","created":"2023-11-06T22:13:21.395513851Z","updated":"2023-11-06T22:13:21.395513919Z"},"test3":{"hash":"test3","dest":"https://cu.bzh/","creator":"gdevillele","updater":"gdevillele","created":"2023-11-06T22:21:24.926289832Z","updated":"2023-11-06T22:21:24.926289906Z"},"ttbrazil":{"hash":"ttbrazil","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-13T10:26:43.132307977Z","updated":"2023-11-13T10:26:43.132308049Z"},"ttfinland":{"hash":"ttfinland","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T15:01:52.245819192Z","updated":"2023-11-06T15:01:52.24581929Z"},"ttfrance":{"hash":"ttfrance","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:05:30.665369218Z","updated":"2023-11-06T14:05:30.665369289Z"},"ttgermany":{"hash":"ttgermany","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:05:55.93776739Z","updated":"2023-11-06T14:05:55.937767462Z"},"ttnetherlands":{"hash":"ttnetherlands","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:07:01.419505051Z","updated":"2023-11-06T14:07:01.41950514Z"},"ttnorway":{"hash":"ttnorway","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:06:43.407607056Z","updated":"2023-11-06T14:06:43.407607131Z"},"ttportugal":{"hash":"ttportugal","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:06:15.27239052Z","updated":"2023-11-06T14:06:15.272390598Z"},"ttsoutheastasia":{"hash":"ttsoutheastasia","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android","creator":"_soliton","updater":"_soliton","created":"2023-11-13T10:33:39.18453545Z","updated":"2023-11-13T10:33:39.184535524Z"},"ttspain":{"hash":"ttspain","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:06:04.262786844Z","updated":"2023-11-06T14:06:04.262786912Z"},"ttsweden":{"hash":"ttsweden","dest":"https://play.google.com/store/apps/details?id=com.voxowl.pcubes.android\u0026hl=en\u0026gl=US","creator":"_soliton","updater":"_soliton","created":"2023-11-06T14:06:50.806188912Z","updated":"2023-11-06T14:06:50.806188993Z"}}}
```

#### Build docker image

```sh
docker build \
  --platform linux/amd64 \
  --file dockerfiles/link.Dockerfile \
  --tag registry.digitalocean.com/cubzh/link:latest \
  .
```

#### Push docker image

```sh
docker push registry.digitalocean.com/cubzh/link:latest
```

#### Update the service

```sh
docker service update --with-registry-auth --image registry.digitalocean.com/cubzh/link:latest link
```

#### Create the service

```sh
docker service create \
--name link \
--mode global \
--constraint node.labels.services-us-1==true \
--mount type=bind,readonly=false,source=/voxowl/link,target=/voxowl/link \
--network cubzh \
--with-registry-auth \
registry.digitalocean.com/cubzh/link:latest
```
