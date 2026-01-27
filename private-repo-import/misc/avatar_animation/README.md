# Avatar animations

## Docker image

### Build

```bash
docker build -t avatar_anim_tools -f ./tools.Dockerfile .
```

### Run

```bash
docker run -ti --rm -v $(pwd):/avatar_anim_tools avatar_anim_tools ash
```

## Python conversion tool

```bash
python convert_json_to_C.py animWalk anim_walk.json

python convert_json_to_C.py animWalkItemRight anim_walkWithItemInHand.json

python convert_json_to_C.py animIdle anim_idle.json

python convert_json_to_C.py animSwingRight anim_swingRight.json

python convert_json_to_C.py animHoldDrink anim_holdDrink.json

python convert_json_to_C.py animDrink anim_drink.json
```