# -*- coding: utf-8 -*-
"""
투사체 정의 XML 을 서버 원본에서 클라이언트로 복제한다.

    python Server/Tools/sync_projectile_data.py

원본 : Server/Config/ProjectileData.xml   (여기만 손으로 고친다)
복제 : Client/Assets/Actor/ProjectileData.xml

서버는 ../Config/ProjectileData.xml 로 직접 읽고,
클라는 ActorData.xml(인덱스)이 가리키는 ../Assets/Actor/ProjectileData.xml 로 읽는다.
두 exe 의 작업 디렉터리가 달라 한 파일을 공유할 수 없으므로 복사한다.

빌드에는 넣지 않는다. 수치를 바꿨을 때만 돌리고 두 파일을 함께 커밋한다.
"""

import os

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(REPO_ROOT, "Server", "Config", "ProjectileData.xml")
DST = os.path.join(REPO_ROOT, "Client", "Assets", "Actor", "ProjectileData.xml")


def main():
    with open(SRC, "r", encoding="utf-8", newline="") as f:
        text = f.read().replace("\r\n", "\n")

    os.makedirs(os.path.dirname(DST), exist_ok=True)
    with open(DST, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)

    print(f"{SRC}\n  -> {DST}")


if __name__ == "__main__":
    main()
