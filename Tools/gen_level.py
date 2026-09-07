# -*- coding: utf-8 -*-
"""
서버 충돌 레벨 XML(Data/Level01.xml)을 클라이언트 프롭 배치에서 생성한다.

    python Server/Tools/gen_level.py

읽는 것 (클라이언트 원본, 손으로 고치는 파일):
    Client/Assets/Level/Cemetery/Cemetery.LevelLayout.xml   프롭 배치 (x/y = 타일 좌상단 셀)
    Client/Assets/Level/Cemetery/CemeteryProps.prop.xml     프롭별 tileSpan / tileSize

쓰는 것:
    Server/Data/Level01.xml   ('#' = 막힘 셀, '.' = 통행 가능)

막힘 셀 :
    1. 각 프롭의 타일 영역 (엔진 StaticPropActor::GetTileBounds 와 동일한 산수)
    2. 아레나 중앙에서 flood-fill 해서 닿지 않는 칸 전부 (링 바깥 여백 + 갇힌 구석).
       울타리 코너에 1칸 틈이 있어도 이걸로 새지 않는다.

★ 프롭 타일 영역 산수 (StaticPropActor::GetTileBounds / TileMapLevel::SpawnProp) ★
    spanInCells = tileSpan * tileSize
    isSide      = facing in (Left, Right)
    w = tileSize     if isSide else spanInCells
    h = spanInCells  if isSide else tileSize
    막힘 사각형 = [x, x+w) x [y, y+h)     (셀 단위, x/y 는 배치 파일의 값 그대로)

빌드에는 넣지 않는다. 배치를 바꿨을 때만 돌리고 결과 XML 을 커밋한다.
convert_level.py 와 같은 이유로 PowerShell 이 아니라 python 으로 쓴다(BOM 없는 LF).
"""

import os
import sys
import xml.etree.ElementTree as ET
from collections import deque

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
LAYOUT_XML = os.path.join(REPO_ROOT, "Client", "Assets", "Level", "Cemetery", "Cemetery.LevelLayout.xml")
PROPS_XML = os.path.join(REPO_ROOT, "Client", "Assets", "Level", "Cemetery", "CemeteryProps.prop.xml")
OUT_XML = os.path.join(REPO_ROOT, "Server", "Data", "Level01.xml")

LEVEL_ID = "Cemetery"
WIDTH = 380
HEIGHT = 280

SIDE_FACINGS = ("Left", "Right")


def load_prop_spans(path):
    """prop 이름 -> tileSpan. tileSize 는 검증만 하고 배치 파일 값을 신뢰한다."""
    root = ET.parse(path).getroot()
    prop_tile_size = int(root.get("tileSize"))
    spans = {}
    for prop in root.findall("Prop"):
        spans[prop.get("name")] = int(prop.get("tileSpan", "1"))
    return spans, prop_tile_size


def load_placements(path):
    root = ET.parse(path).getroot()
    tile_size = int(root.get("tileSize"))
    out = []
    for p in root.findall("Prop"):
        out.append((
            p.get("name"),
            int(p.get("x")),
            int(p.get("y")),
            p.get("facing", "Up"),
        ))
    return out, tile_size


def prop_rect(name, x, y, facing, span, tile_size):
    span_cells = span * tile_size
    is_side = facing in SIDE_FACINGS
    w = tile_size if is_side else span_cells
    h = span_cells if is_side else tile_size
    return x, y, w, h


def main():
    spans, prop_tile_size = load_prop_spans(PROPS_XML)
    placements, layout_tile_size = load_placements(LAYOUT_XML)

    if prop_tile_size != layout_tile_size:
        sys.exit(f"tileSize mismatch: props={prop_tile_size} layout={layout_tile_size}")
    tile_size = layout_tile_size

    grid = [[False] * WIDTH for _ in range(HEIGHT)]

    def block_rect(x0, y0, w, h):
        for yy in range(max(0, y0), min(HEIGHT, y0 + h)):
            row = grid[yy]
            for xx in range(max(0, x0), min(WIDTH, x0 + w)):
                row[xx] = True

    # 1. 프롭 타일 영역
    unknown = set()
    for name, x, y, facing in placements:
        if name not in spans:
            unknown.add(name)
            continue
        rx, ry, rw, rh = prop_rect(name, x, y, facing, spans[name], tile_size)
        block_rect(rx, ry, rw, rh)

    if unknown:
        sys.exit(f"placement refers to props missing from prop.xml: {sorted(unknown)}")

    # 2. 아레나 중앙에서 flood-fill. 닿지 않는 칸을 전부 막는다
    #    (링 바깥 여백 + 코너 틈으로 샌 자투리 + 갇힌 구석).
    seed = (WIDTH // 2, HEIGHT // 2)
    if grid[seed[1]][seed[0]]:
        sys.exit(f"flood seed {seed} is inside a prop - pick another")

    reachable = [[False] * WIDTH for _ in range(HEIGHT)]
    q = deque([seed])
    reachable[seed[1]][seed[0]] = True
    while q:
        cx, cy = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT \
                    and not reachable[ny][nx] and not grid[ny][nx]:
                reachable[ny][nx] = True
                q.append((nx, ny))

    for yy in range(HEIGHT):
        row_r = reachable[yy]
        row_g = grid[yy]
        for xx in range(WIDTH):
            if not row_r[xx]:
                row_g[xx] = True

    blocked_count = sum(r.count(True) for r in grid)
    reachable_count = sum(r.count(True) for r in reachable)

    lines = []
    lines.append(
        f'<Level levelId="{LEVEL_ID}" width="{WIDTH}" height="{HEIGHT}" tileSize="{tile_size}">'
    )
    for row in grid:
        lines.append("\t<Row>" + "".join("#" if c else "." for c in row) + "</Row>")
    lines.append("</Level>")
    lines.append("")

    with open(OUT_XML, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print(f"wrote {OUT_XML}")
    print(f"  {WIDTH} x {HEIGHT} cells, tileSize {tile_size}")
    print(f"  {len(placements)} props")
    print(f"  {reachable_count} walkable cells, {blocked_count} blocked "
          f"({100.0 * blocked_count / (WIDTH * HEIGHT):.1f}%)")


if __name__ == "__main__":
    main()
