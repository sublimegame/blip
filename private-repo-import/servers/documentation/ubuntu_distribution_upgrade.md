# Ubuntu distribution upgrade

## Steps to upgrade from 22.04 to 24.04

Update 22.04 LTS packages

```bash
sudo apt update && sudo apt upgrade -y
sudo reboot
```

Make sure 1022 port is open in case we need a backup port for SSH.
(this is not needed if you have physical access to the machine)

```bash
sudo ufw allow 1022/tcp
```

Make sure `update-manager-core` package is installed and up-to-date

```bash
sudo apt install update-manager-core
```

Upgrade to 24.04 LTS

```bash
sudo do-release-upgrade
```

Remove obsolete packages

```bash
sudo apt autoremove
```
